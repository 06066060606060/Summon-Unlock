#define CAN_TX_PIN       5
#define CAN_RX_PIN       6


#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Update.h>
#include "driver/twai.h"
#include "index_html.h"

static volatile bool forceMode = false;

static portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;

static Preferences   prefs;
static volatile bool summonEnabled = true;
static volatile bool tlsscEnabled  = false;   // "Enable TLSSC" - off by default
static volatile bool gtwSdEnabled  = false;   // "GTW SELF_DRIVING" - off by default
static volatile bool tlsscRestoreEnabled = false;   // "TLSSC Restore" (0x331) - off by default

static volatile bool gateAPActive  = false;
static volatile bool gateParked    = true;
static volatile bool gateSummoning = false;

static volatile bool sprSeen  = false;
static volatile bool lastAca  = false;

#define PARKED_TIMEOUT_MS  5000
static volatile uint32_t last280Millis = 0;

static volatile uint32_t rxMux1   = 0;
static volatile uint32_t txOk     = 0;
static volatile uint32_t txFail   = 0;
static volatile uint32_t rx280    = 0;
static volatile uint32_t rx390    = 0;
static volatile uint32_t rx921    = 0;
static volatile uint32_t rx1016   = 0;
static unsigned long     bootTime = 0;

static char gateBlockReason[48] = "boot";

// ── Force inject / force mode ─────────────────────────────────

static uint8_t       last1021Data[8] = {0};
static volatile bool last1021Valid = false;
// ═══════════════════════════════════════════════════════════════
// HELPERS CAN
// ═══════════════════════════════════════════════════════════════

static inline uint8_t readMuxID(const uint8_t *data) {
    return data[0] & 0x07;
}

static inline bool getBit(const uint8_t *data, int bit) {
    return (data[bit / 8] >> (bit % 8)) & 0x01;
}
static inline void setBit(uint8_t *data, int bit, bool val) {
    uint8_t mask = (uint8_t)(1U << (bit % 8));
    if (val) data[bit / 8] |=  mask;
    else     data[bit / 8] &= ~mask;
}

static inline uint8_t readDIGear(const uint8_t *data) {
    return (data[2] >> 5) & 0x07;
}

static inline uint8_t readVehicleGear(const uint8_t *data) {
    return (data[2] >> 5) & 0x07;
}

static inline int gearState(uint8_t gear) {
    if (gear == 1)             return  1;
    if (gear == 2 || gear == 3 || gear == 4) return 0;
    return -1;
}

static inline uint8_t readDASStatus(const uint8_t *data) {
    return data[0] & 0x07;
}
// AP « actif » pour status 3,4,5,6
static inline bool isDASActive(uint8_t status) {
  bool fm = forceMode;

  switch (status) {
    // ON : 3,4,5,6
    case 3:
    case 4:
    case 5:
    case 6:
      fm = true;
      break;

    // OFF : 0,1,8,9,14
    case 0:
    case 1:
    case 8:
    case 9:
    case 14:
      fm = false;
      break;

    default:
      fm = false;
      break;
  }


  portENTER_CRITICAL(&stateMux);
  forceMode = fm;
  portEXIT_CRITICAL(&stateMux);


  return status == 3 || status == 4 || status == 5 || status == 6;
}



// ═══════════════════════════════════════════════════════════════
// LOGIQUE GATE
// ═══════════════════════════════════════════════════════════════

static inline bool injectionGateOpen() {
    return gateParked || gateSummoning;
}

static void recomputeSummoning() {
    gateSummoning = lastAca && sprSeen;
}

static void clearSummonOnPark() {
    gateSummoning = false;
    sprSeen       = false;
}

static void clearSummonOnParkIfAcaInactive(uint8_t gear) {
    if (gear == 1 && !lastAca)
        clearSummonOnPark();
}

static void handle280(const uint8_t *data) {
    rx280++;
    last280Millis = (uint32_t)millis();
    uint8_t gear = readDIGear(data);
    int     gs   = gearState(gear);

    portENTER_CRITICAL(&stateMux);
    if (gs == 1)  gateParked = true;
    if (gs == 0)  gateParked = false;

    bool aca = (data[6] & 0x04) != 0;
    if (lastAca && !aca)
        sprSeen = false;
    lastAca = aca;
    recomputeSummoning();

    clearSummonOnParkIfAcaInactive(gear);
    portEXIT_CRITICAL(&stateMux);
}

static void handle390(const uint8_t *data) {
    rx390++;
    uint8_t gear = readVehicleGear(data);
    int     gs   = gearState(gear);
    if (gs < 0) return;

    portENTER_CRITICAL(&stateMux);
    uint32_t age = (uint32_t)millis() - last280Millis;
    if (last280Millis == 0 || age > PARKED_TIMEOUT_MS) {
        gateParked = (gs == 1);
        clearSummonOnParkIfAcaInactive(gear);
    }
    portEXIT_CRITICAL(&stateMux);
}

static void handle921(const uint8_t *data) {
    rx921++;
    bool ap = isDASActive(readDASStatus(data));

    portENTER_CRITICAL(&stateMux);
    gateAPActive = ap;
    portEXIT_CRITICAL(&stateMux);
}

static void handle1016(const uint8_t *data, uint8_t dlc) {
    if (dlc < 4) return;
    rx1016++;
    uint8_t spr = (data[3] >> 4) & 0x0F;

    portENTER_CRITICAL(&stateMux);
    if (spr != 0)
        sprSeen = true;
    recomputeSummoning();
    portEXIT_CRITICAL(&stateMux);
}

// ═══════════════════════════════════════════════════════════════
// INJECTION SUMMON
// ═══════════════════════════════════════════════════════════════

static void doInjectSummon(const uint8_t *srcData, uint8_t dlc) {
    if (dlc < 8) return;
    twai_message_t out;
    out.identifier       = 1021;
    out.data_length_code = 8;
    out.flags            = 0;
    memcpy(out.data, srcData, 8);

    setBit(out.data, 19, false);
    setBit(out.data, 47, true);

    rxMux1++;
    esp_err_t err = twai_transmit(&out, pdMS_TO_TICKS(2));
    if (err == ESP_OK) txOk++;
    else               txFail++;
}

// ── TLSSC : 0x3FD mux0 bit38 -> UI_fsdStopsControlEnabled = 1 ──
static void doInjectTLSSC(const uint8_t *srcData, uint8_t dlc) {
    if (dlc < 8) return;
    twai_message_t out;
    out.identifier       = 1021;
    out.data_length_code = 8;
    out.flags            = 0;
    memcpy(out.data, srcData, 8);

    setBit(out.data, 38, true);   // UI_fsdStopsControlEnabled = 1

    esp_err_t err = twai_transmit(&out, pdMS_TO_TICKS(2));
    if (err == ESP_OK) txOk++;
    else               txFail++;
}

static void injectTLSSC(const twai_message_t &src) {
    bool en, gate, fmode;
    portENTER_CRITICAL(&stateMux);
    en    = tlsscEnabled;
    gate  = injectionGateOpen();
    fmode = forceMode;
    portEXIT_CRITICAL(&stateMux);

    if ((!en || !gate) && !fmode)
        return;

    doInjectTLSSC(src.data, src.data_length_code);
}

// ── GTW config replay : 0x7FF (GTW_carConfig) mux2, GTW_autopilot -> SELF_DRIVING(3) ──
// DBC: SG_ GTW_autopilot m2 : 42|3@1+  ; VAL_ 3 "SELF_DRIVING"
static void doInjectGtwConfig(const uint8_t *srcData, uint8_t dlc) {
    if (dlc < 8) return;
    twai_message_t out;
    out.identifier       = 2047;   // 0x7FF
    out.data_length_code = 8;
    out.flags            = 0;
    memcpy(out.data, srcData, 8);

    // GTW_autopilot = 3 (SELF_DRIVING) : 3 bits little-endian a partir du bit 42
    setBit(out.data, 42, true);    // bit0 of value -> 1
    setBit(out.data, 43, true);    // bit1 of value -> 1
    setBit(out.data, 44, false);   // bit2 of value -> 0   => 0b011 = 3

    esp_err_t err = twai_transmit(&out, pdMS_TO_TICKS(2));
    if (err == ESP_OK) txOk++;
    else               txFail++;
}

// ── TLSSC Restore : 0x331 (DAS_autopilotConfig) DAS_autopilot & DAS_autopilotBase -> SELF_DRIVING(3) ──
// RE (flipper-tesla-fsd): write byte[0] low 6 bits = 0x1B  => both 3-bit fields = 3 (SELF_DRIVING)
static void doInjectTlsscRestore(const uint8_t *srcData, uint8_t dlc) {
    if (dlc < 1) return;
    twai_message_t out;
    out.identifier       = 817;   // 0x331
    out.data_length_code = dlc;
    out.flags            = 0;
    memcpy(out.data, srcData, dlc);

    // Preserve top 2 bits of byte0, force low 6 bits to 0x1B
    out.data[0] = (uint8_t)((out.data[0] & 0xC0) | 0x1B);

    esp_err_t err = twai_transmit(&out, pdMS_TO_TICKS(2));
    if (err == ESP_OK) txOk++;
    else               txFail++;
}

static void injectTlsscRestore(const twai_message_t &src) {
    bool en, gate, fmode;
    portENTER_CRITICAL(&stateMux);
    en    = tlsscRestoreEnabled;
    gate  = injectionGateOpen();
    fmode = forceMode;
    portEXIT_CRITICAL(&stateMux);

    if ((!en || !gate) && !fmode)
        return;

    doInjectTlsscRestore(src.data, src.data_length_code);
}

static void injectGtwConfig(const twai_message_t &src) {
    bool en, gate, fmode;
    portENTER_CRITICAL(&stateMux);
    en    = gtwSdEnabled;
    gate  = injectionGateOpen();
    fmode = forceMode;
    portEXIT_CRITICAL(&stateMux);

    if ((!en || !gate) && !fmode)
        return;

    doInjectGtwConfig(src.data, src.data_length_code);
}

static void injectSummon(const twai_message_t &src) {
    bool en, gate, fmode;
    portENTER_CRITICAL(&stateMux);
    en    = summonEnabled;
    gate  = injectionGateOpen();
    fmode = forceMode;
    if (!gate && !fmode) {
        if (!gateAPActive && !gateParked && !gateSummoning)
            strncpy(gateBlockReason, "AP-,Park-,Summon-", sizeof(gateBlockReason));
    }
    portEXIT_CRITICAL(&stateMux);

    if ((!en || !gate) && !fmode)
        return;

    doInjectSummon(src.data, src.data_length_code);
}

// ═══════════════════════════════════════════════════════════════
//  CAN (Core 1, HIGH PRIORITY)
// ═══════════════════════════════════════════════════════════════

static const uint32_t WATCH_IDS[] = {280, 390, 921, 1016, 1021};

static void canTask(void *arg) {
    for (;;) {
        twai_message_t f;
        while (twai_receive(&f, pdMS_TO_TICKS(2)) == ESP_OK) {
            switch (f.identifier) {
                case 280:
                    if (f.data_length_code >= 7) handle280(f.data);
                    break;
                case 390:
                    if (f.data_length_code >= 8) handle390(f.data);
                    break;
                case 921:
                    if (f.data_length_code >= 1) handle921(f.data);
                    break;
                case 1016:
                    handle1016(f.data, f.data_length_code);
                    break;
                case 1021:
                    if (f.data_length_code >= 8) {
                        uint8_t mux = readMuxID(f.data);
                        if (mux == 1) {
                            portENTER_CRITICAL(&stateMux);
                            memcpy(last1021Data, f.data, 8);
                            last1021Valid = true;
                            portEXIT_CRITICAL(&stateMux);
                            injectSummon(f);
                        } else if (mux == 0) {
                            injectTLSSC(f);
                        }
                    }
                    break;
                case 2047:   // 0x7FF GTW_carConfig - mux2 carries GTW_autopilot
                    if (f.data_length_code >= 8 && f.data[0] == 2)
                        injectGtwConfig(f);
                    break;
                case 817:    // 0x331 DAS_autopilotConfig - TLSSC Restore
                    injectTlsscRestore(f);
                    break;
                default:
                    break;
            }
        }

        twai_status_info_t st;
        twai_get_status_info(&st);
        if (st.state == TWAI_STATE_BUS_OFF) {
            twai_initiate_recovery();
            vTaskDelay(pdMS_TO_TICKS(300));
        }

        uint32_t now = (uint32_t)millis();
        portENTER_CRITICAL(&stateMux);
        bool can280Stale = (last280Millis > 0) &&
                           (now - last280Millis > PARKED_TIMEOUT_MS);
        if (can280Stale)
            gateParked = true;
        portEXIT_CRITICAL(&stateMux);

        vTaskDelay(1);
    }
}

// ═══════════════════════════════════════════════════════════════
// DASHBOARD WI-FI
// ═══════════════════════════════════════════════════════════════

extern const char INDEX_HTML[] PROGMEM;
static WebServer server(80);

#define FW_VERSION "V2.3b"

static volatile bool     otaInProgress = false;
static volatile bool     otaSuccess    = false;
static volatile bool     otaError      = false;
static volatile uint32_t otaBytes      = 0;
static volatile uint32_t otaTotal      = 0;
static char              otaErrMsg[64] = "";

static void cfgLoad() {
    prefs.begin("summon", true);
    summonEnabled = prefs.getBool("en", true);
    tlsscEnabled  = prefs.getBool("tlssc", false);
    gtwSdEnabled  = prefs.getBool("gtwsd", false);
    tlsscRestoreEnabled = prefs.getBool("tlrst", false);
    prefs.end();
}

static void cfgSave() {
    prefs.begin("summon", false);
    prefs.putBool("en", summonEnabled);
    prefs.putBool("tlssc", tlsscEnabled);
    prefs.putBool("gtwsd", gtwSdEnabled);
    prefs.putBool("tlrst", tlsscRestoreEnabled);
    prefs.end();
}

static String statsToJson() {
    bool en, tlssc, gtwsd, tlrst, ap, parked, summon, aca, spr, fmode, l1021;
    uint32_t rmx, tok, tfail, r280, r390, r921, r1016;

    portENTER_CRITICAL(&stateMux);
    en     = summonEnabled;
    tlssc  = tlsscEnabled;
    gtwsd  = gtwSdEnabled;
    tlrst  = tlsscRestoreEnabled;
    ap     = gateAPActive;
    parked = gateParked;
    summon = gateSummoning;
    aca    = lastAca;
    spr    = sprSeen;
    rmx    = rxMux1;
    tok    = txOk;
    tfail  = txFail;
    r280   = rx280;
    r390   = rx390;
    r921   = rx921;
    r1016  = rx1016;
    fmode  = forceMode;
    l1021  = last1021Valid;
    portEXIT_CRITICAL(&stateMux);

    bool gate = parked || summon;

    twai_status_info_t st; twai_get_status_info(&st);

    String s = "{";
    s += "\"enabled\":"  + String(en     ? "true" : "false");
    s += ",\"tlssc\":"   + String(tlssc  ? "true" : "false");
    s += ",\"gtwsd\":"   + String(gtwsd  ? "true" : "false");
    s += ",\"tlrst\":"   + String(tlrst  ? "true" : "false");
    s += ",\"gate\":"    + String(gate   ? "true" : "false");
    s += ",\"ap\":"      + String(ap     ? "true" : "false");
    s += ",\"parked\":"  + String(parked ? "true" : "false");
    s += ",\"summon\":"  + String(summon ? "true" : "false");
    s += ",\"aca\":"     + String(aca    ? "true" : "false");
    s += ",\"spr\":"     + String(spr    ? "true" : "false");
    s += ",\"forceMode\":"+ String(fmode ? "true" : "false");
    s += ",\"last1021\":"+ String(l1021 ? "true" : "false");
    s += ",\"rxMux1\":"  + String(rmx);
    s += ",\"txOk\":"    + String(tok);
    s += ",\"txFail\":"  + String(tfail);
    s += ",\"rx280\":"   + String(r280);
    s += ",\"rx390\":"   + String(r390);
    s += ",\"rx921\":"   + String(r921);
    s += ",\"rx1016\":"  + String(r1016);
    s += ",\"canState\":" + String((int)st.state);
    s += ",\"uptimeS\":"  + String((millis() - bootTime) / 1000);
    s += ",\"fwVersion\":\"" + String(FW_VERSION) + "\"";
    s += ",\"otaInProgress\":" + String(otaInProgress ? "true" : "false");
    s += ",\"otaSuccess\":"    + String(otaSuccess    ? "true" : "false");
    s += ",\"otaError\":"      + String(otaError      ? "true" : "false");
    s += ",\"otaErrMsg\":\""   + String(otaErrMsg) + "\"";
    s += ",\"otaBytes\":"      + String(otaBytes);
    s += ",\"otaTotal\":"      + String(otaTotal);
    s += ",\"freeHeap\":"      + String(ESP.getFreeHeap());
    s += "}";
    return s;
}

static void httpRoot()   { server.send_P(200, "text/html", INDEX_HTML); }
static void httpStats()  { server.send(200, "application/json", statsToJson()); }

static void httpEnable() {
    portENTER_CRITICAL(&stateMux); summonEnabled = true;  portEXIT_CRITICAL(&stateMux);
    cfgSave();
    server.send(200, "application/json", statsToJson());
}
static void httpDisable() {
    portENTER_CRITICAL(&stateMux); summonEnabled = false; portEXIT_CRITICAL(&stateMux);
    cfgSave();
    server.send(200, "application/json", statsToJson());
}

static void httpTlsscEnable() {
    portENTER_CRITICAL(&stateMux); tlsscEnabled = true;  portEXIT_CRITICAL(&stateMux);
    cfgSave();
    server.send(200, "application/json", statsToJson());
}
static void httpTlsscDisable() {
    portENTER_CRITICAL(&stateMux); tlsscEnabled = false; portEXIT_CRITICAL(&stateMux);
    cfgSave();
    server.send(200, "application/json", statsToJson());
}
static void httpGtwEnable() {
    portENTER_CRITICAL(&stateMux); gtwSdEnabled = true;  portEXIT_CRITICAL(&stateMux);
    cfgSave();
    server.send(200, "application/json", statsToJson());
}
static void httpGtwDisable() {
    portENTER_CRITICAL(&stateMux); gtwSdEnabled = false; portEXIT_CRITICAL(&stateMux);
    cfgSave();
    server.send(200, "application/json", statsToJson());
}
static void httpTlrstEnable() {
    portENTER_CRITICAL(&stateMux); tlsscRestoreEnabled = true;  portEXIT_CRITICAL(&stateMux);
    cfgSave();
    server.send(200, "application/json", statsToJson());
}
static void httpTlrstDisable() {
    portENTER_CRITICAL(&stateMux); tlsscRestoreEnabled = false; portEXIT_CRITICAL(&stateMux);
    cfgSave();
    server.send(200, "application/json", statsToJson());
}

static void httpForce() {
    uint8_t data[8];
    bool valid;
    portENTER_CRITICAL(&stateMux);
    valid = last1021Valid;
    if (valid) memcpy(data, last1021Data, 8);
    portEXIT_CRITICAL(&stateMux);

    if (valid) {
        doInjectSummon(data, 8);
    } else {
        uint8_t fallback[8] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00};
        doInjectSummon(fallback, 8);
    }
    server.send(200, "application/json", statsToJson());
}

static void httpForceMode() {
    portENTER_CRITICAL(&stateMux);
    forceMode = !forceMode;
    portEXIT_CRITICAL(&stateMux);
    server.send(200, "application/json", statsToJson());
}

// ─── OTA update ─────────────────────────────────────────────

static void httpOtaUpload() {
    HTTPUpload &up = server.upload();

    if (up.status == UPLOAD_FILE_START) {
        otaInProgress = true;
        otaSuccess    = false;
        otaError      = false;
        otaBytes      = 0;
        otaErrMsg[0]  = '\0';
        Serial.printf("[OTA] Start: %s\n", up.filename.c_str());

        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            otaError = true;
            strncpy(otaErrMsg, Update.errorString(), sizeof(otaErrMsg) - 1);
            Serial.printf("[OTA] begin() failed: %s\n", otaErrMsg);
        }
    } else if (up.status == UPLOAD_FILE_WRITE) {
        if (!otaError && Update.write(up.buf, up.currentSize) != up.currentSize) {
            otaError = true;
            strncpy(otaErrMsg, Update.errorString(), sizeof(otaErrMsg) - 1);
            Serial.printf("[OTA] write() failed: %s\n", otaErrMsg);
        }
        otaBytes += up.currentSize;
    } else if (up.status == UPLOAD_FILE_END) {
        if (!otaError && Update.end(true)) {
            otaSuccess = true;
            otaTotal   = otaBytes;
            Serial.printf("[OTA] Success: %u bytes\n", up.totalSize);
        } else if (!otaError) {
            otaError = true;
            strncpy(otaErrMsg, Update.errorString(), sizeof(otaErrMsg) - 1);
            Serial.printf("[OTA] end() failed: %s\n", otaErrMsg);
        }
        otaInProgress = false;
    } else if (up.status == UPLOAD_FILE_ABORTED) {
        Update.end();
        otaInProgress = false;
        otaError      = true;
        strncpy(otaErrMsg, "aborted", sizeof(otaErrMsg) - 1);
        Serial.println("[OTA] Aborted");
    }
}

static void httpOtaFinish() {
    bool ok = otaSuccess && !otaError;
    String resp = String("{\"ok\":") + (ok ? "true" : "false") +
                  ",\"error\":\"" + String(otaErrMsg) + "\"}";
    server.sendHeader("Connection", "close");
    server.send(200, "application/json", resp);
    if (ok) {
        delay(700);
        ESP.restart();
    }
}

static void webTask(void *arg) {
    WiFi.mode(WIFI_AP);
    uint8_t mac[6]; WiFi.softAPmacAddress(mac);
    char ssid[28];
    snprintf(ssid, sizeof(ssid), "SummonUnlock-%02X%02X", mac[4], mac[5]);
    WiFi.softAP(ssid, "summon1234");
    Serial.printf("[WIFI] SSID=%s  PASS=summon1234  IP=%s\n",
                  ssid, WiFi.softAPIP().toString().c_str());

    server.on("/",            HTTP_GET,  httpRoot);
    server.on("/api/stats",   HTTP_GET,  httpStats);
    server.on("/api/enable",  HTTP_POST, httpEnable);
    server.on("/api/disable", HTTP_POST, httpDisable);
    server.on("/api/tlssc-enable",  HTTP_POST, httpTlsscEnable);
    server.on("/api/tlssc-disable", HTTP_POST, httpTlsscDisable);
    server.on("/api/gtwsd-enable",  HTTP_POST, httpGtwEnable);
    server.on("/api/gtwsd-disable", HTTP_POST, httpGtwDisable);
    server.on("/api/tlrst-enable",  HTTP_POST, httpTlrstEnable);
    server.on("/api/tlrst-disable", HTTP_POST, httpTlrstDisable);
    server.on("/api/force",   HTTP_POST, httpForce);
    server.on("/api/forcemode", HTTP_POST, httpForceMode);
    server.on("/update", HTTP_POST, httpOtaFinish, httpOtaUpload);
    server.begin();
    for (;;) { server.handleClient(); vTaskDelay(1); }
}

// ═══════════════════════════════════════════════════════════════
// SETUP / LOOP
// ═══════════════════════════════════════════════════════════════

void setup() {
    bootTime = millis();
    Serial.begin(115200);
    delay(500);
    Serial.printf("IDF: %s\n", esp_get_idf_version());

    cfgLoad();

   

    twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN, TWAI_MODE_NORMAL);
    g.rx_queue_len = 64;
    g.tx_queue_len = 16;
    twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    Serial.printf("twai_install=%d  twai_start=%d\n",
                  (int)twai_driver_install(&g, &t, &f),
                  (int)twai_start());

    Serial.println("=== SummonUnlock ready ===");
    Serial.println("  Injection gate : Parked || Summoning");
    Serial.println("  CAN 1021 mux1  : bit19->0, bit47->1");
    Serial.printf ("  summonEnabled  : %s\n", summonEnabled ? "true" : "false");
    Serial.printf ("  tlsscEnabled   : %s (0x3FD mux0 bit38)\n", tlsscEnabled ? "true" : "false");
    Serial.printf ("  gtwSdEnabled   : %s (0x7FF mux2 GTW_autopilot=3)\n", gtwSdEnabled ? "true" : "false");
    Serial.printf ("  tlsscRestore   : %s (0x331 byte0 low6=0x1B)\n", tlsscRestoreEnabled ? "true" : "false");

    xTaskCreatePinnedToCore(canTask, "can", 4096,  nullptr, 5, nullptr, 1);
    xTaskCreatePinnedToCore(webTask, "web", 8192,  nullptr, 1, nullptr, 0);
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(50));
}
