//UPDATE V2.0
// - bypass EU restriction in Autopilot (EAP).  (no confirmation needed to exit).
// - smoother braking



#define CAN_TX_PIN       5
#define CAN_RX_PIN       6


#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "driver/twai.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "index_html.h"

static volatile bool forceMode = false;

static portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;

static Preferences   prefs;
static volatile bool summonEnabled = true;

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
                    if (f.data_length_code >= 8 && readMuxID(f.data) == 1) {
                        portENTER_CRITICAL(&stateMux);
                        memcpy(last1021Data, f.data, 8);
                        last1021Valid = true;
                        portEXIT_CRITICAL(&stateMux);
                        injectSummon(f);
                    }
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
// BLE GATT
// ═══════════════════════════════════════════════════════════════

#define BLE_SERVICE_UUID   "12345678-1234-1234-1234-123456789abc"
#define BLE_CHAR_CTRL_UUID "12345678-1234-1234-1234-123456789001"
#define BLE_CHAR_STAT_UUID "12345678-1234-1234-1234-123456789002"

static BLECharacteristic *bleStatChar = nullptr;
static volatile bool      bleConnected = false;

class BleServerCb : public BLEServerCallbacks {
    void onConnect(BLEServer *)    override {
        bleConnected = true;
        Serial.println("[BLE] Client connecté");
    }
    void onDisconnect(BLEServer *s) override {
        bleConnected = false;
        Serial.println("[BLE] Client déconnecté — re-advertising");
        s->startAdvertising();
    }
};

class BleCtrlCb : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *c) override {
        String val = c->getValue().c_str();
        bool next = (val == "1" || val == "true" || val == "on");
        portENTER_CRITICAL(&stateMux);
        summonEnabled = next;
        portEXIT_CRITICAL(&stateMux);
        cfgSave();
        Serial.printf("[BLE] summonEnabled → %s\n", next ? "true" : "false");
    }
};

static void bleSetup() {
    BLEDevice::init("SummonUnlock");
    BLEServer *srv = BLEDevice::createServer();
    srv->setCallbacks(new BleServerCb());

    BLEService *svc = srv->createService(BLE_SERVICE_UUID);

    BLECharacteristic *ctrlChar = svc->createCharacteristic(
        BLE_CHAR_CTRL_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    ctrlChar->setCallbacks(new BleCtrlCb());

    bleStatChar = svc->createCharacteristic(
        BLE_CHAR_STAT_UUID,
        BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
    );
    bleStatChar->addDescriptor(new BLE2902());

    svc->start();

    BLEAdvertising *adv = BLEDevice::getAdvertising();
    adv->addServiceUUID(BLE_SERVICE_UUID);
    adv->setScanResponse(true);
    adv->setMinPreferred(0x06);
    BLEDevice::startAdvertising();
    Serial.println("[BLE] Advertising — SummonUnlock");
}

static void bleTask(void *arg) {
    for (;;) {
        if (bleConnected && bleStatChar) {
            String j = statsToJson();
            bleStatChar->setValue(j.c_str());
            bleStatChar->notify();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ═══════════════════════════════════════════════════════════════
// DASHBOARD WI-FI
// ═══════════════════════════════════════════════════════════════

extern const char INDEX_HTML[] PROGMEM;
static WebServer server(80);

static void cfgLoad() {
    prefs.begin("summon", true);
    summonEnabled = prefs.getBool("en", true);
    prefs.end();
}

static void cfgSave() {
    prefs.begin("summon", false);
    prefs.putBool("en", summonEnabled);
    prefs.end();
}

static String statsToJson() {
    bool en, ap, parked, summon, aca, spr, fmode, l1021;
    uint32_t rmx, tok, tfail, r280, r390, r921, r1016;

    portENTER_CRITICAL(&stateMux);
    en     = summonEnabled;
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
    server.on("/api/force",   HTTP_POST, httpForce);
    server.on("/api/forcemode", HTTP_POST, httpForceMode);
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

    xTaskCreatePinnedToCore(canTask, "can", 4096,  nullptr, 5, nullptr, 1);
    xTaskCreatePinnedToCore(webTask, "web", 8192,  nullptr, 1, nullptr, 0);

    bleSetup();
    xTaskCreatePinnedToCore(bleTask, "ble", 4096,  nullptr, 1, nullptr, 0);
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(50));
}
