/*
 * SummonUnlock — Arduino IDE (ESP32 + TWAI natif)
 *
 * Logique complète portée depuis handlers.h :
 *  - Injection gate  : Parked || Summoning
 *  - Détection ACA   : CAN 280  bit 50  (DI_autonomyControlActive)
 *  - Détection SPR   : CAN 1016 data[3] bits 4-7 (UI_selfParkRequest)
 *  - Gear Parked     : CAN 280  (DI_gear) + CAN 390 (DIF_gear)
 *  - Inject mux 1    : CAN 1021, bit 19→0, bit 47→1  (HW4)
 *
 * Connectivité :
 *  - Wi-Fi AP   : dashboard HTML complet  → http://192.168.4.1
 *  - BLE GATT   : contrôle depuis Web Bluetooth (Chrome Android / page GitHub Pages)
 *
 * BLE UUIDs :
 *  Service  : 12345678-1234-1234-1234-123456789abc
 *  CTRL     : 12345678-1234-1234-1234-123456789001  (write  : "1"=enable "0"=disable)
 *  STATS    : 12345678-1234-1234-1234-123456789002  (notify : JSON stats ~1s)
 *
 * ── Pins ──────────────────────────────────────────
 * Modifier selon ton câblage :
 */
#define CAN_TX_PIN  5
#define CAN_RX_PIN  6

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "driver/twai.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ═══════════════════════════════════════════════════════════════
// HELPERS CAN
// ═══════════════════════════════════════════════════════════════

// Lit les bits 0-2 de data[0] → identifiant mux
static inline uint8_t readMuxID(const uint8_t *data) {
    return data[0] & 0x07;
}

// Lit/écrit un bit dans un payload 8 octets (bit 0 = LSB de data[0])
static inline bool getBit(const uint8_t *data, int bit) {
    return (data[bit / 8] >> (bit % 8)) & 0x01;
}
static inline void setBit(uint8_t *data, int bit, bool val) {
    uint8_t mask = (uint8_t)(1U << (bit % 8));
    if (val) data[bit / 8] |=  mask;
    else     data[bit / 8] &= ~mask;
}

// ── CAN 280 : DI_gear (bits 21-23 = data[2] bits 5-7) ─────────
// Valeurs : 0=INVALID, 1=P, 2=R, 3=N, 4=D, 7=SNA
static inline uint8_t readDIGear(const uint8_t *data) {
    return (data[2] >> 5) & 0x07;
}

// ── CAN 390 : DIF_gear (bits 21-23 = data[2] bits 5-7) ────────
static inline uint8_t readVehicleGear(const uint8_t *data) {
    return (data[2] >> 5) & 0x07;
}

// gear==1 → Park.
// gear 0 (INVALID) et 7 (SNA) → indéterminé, on ignore (retourne -1)
// gear 2/3/4 (R/N/D) → roulage confirmé
static inline int gearState(uint8_t gear) {
    if (gear == 1)             return  1;   // Park confirmé
    if (gear == 2 || gear == 3 || gear == 4) return 0;   // Roulage confirmé
    return -1;                               // INVALID/SNA → ignorer
}

// ── CAN 921 : DAS_autopilotStatus (bits 0-2 de data[0]) ───────
static inline uint8_t readDASStatus(const uint8_t *data) {
    return data[0] & 0x07;
}
// 2=ACTIVE_NOMINAL, 3=ACTIVE_RESTRICTED, 4=ACTIVE_NAV
static inline bool isDASActive(uint8_t status) {
    return status == 2 || status == 3 || status == 4;
}

// ═══════════════════════════════════════════════════════════════
// ÉTAT GLOBAL (thread-safe via portMUX)
// ═══════════════════════════════════════════════════════════════

static portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;

// ── Config persistée ───────────────────────────────────────────
static Preferences   prefs;
static volatile bool summonEnabled = true;   // toggle dashboard

// ── Injection gate ─────────────────────────────────────────────
static volatile bool gateAPActive  = false;  // CAN 921
static volatile bool gateParked    = true;   // CAN 280/390 — true par défaut (boot)
static volatile bool gateSummoning = false;  // ACA && sprSeen

// ── Summon discrimination (ACA + SPR) ─────────────────────────
static volatile bool sprSeen  = false;  // UI_selfParkRequest vu pendant cet épisode
static volatile bool lastAca  = false;  // DI_autonomyControlActive précédent

// ── Timeout watchdog CAN 280 ─────────────────────────────────
// Si CAN 280 n'arrive plus (bus silence), on repasse gateParked=true
// après PARKED_TIMEOUT_MS pour ne pas bloquer le summon.
#define PARKED_TIMEOUT_MS  5000
static volatile uint32_t last280Millis = 0;

// ── Stats ──────────────────────────────────────────────────────
static volatile uint32_t rxMux1   = 0;   // frames CAN 1021 mux1 reçues
static volatile uint32_t txOk     = 0;   // injections réussies
static volatile uint32_t txFail   = 0;   // échecs TWAI
static volatile uint32_t rx280    = 0;
static volatile uint32_t rx390    = 0;
static volatile uint32_t rx921    = 0;
static volatile uint32_t rx1016   = 0;
static unsigned long     bootTime = 0;

// ── Raison du blocage (debug) ──────────────────────────────────
static char gateBlockReason[48] = "boot";

// ═══════════════════════════════════════════════════════════════
// LOGIQUE GATE
// ═══════════════════════════════════════════════════════════════

static inline bool injectionGateOpen() {
    return gateParked || gateSummoning;
}

// Recalcule gateSummoning depuis lastAca + sprSeen
static void recomputeSummoning() {
    gateSummoning = lastAca && sprSeen;
}

// Appelé quand ACA tombe ET gear==P sans ACA → reset complet de l'épisode
static void clearSummonOnPark() {
    gateSummoning = false;
    sprSeen       = false;
}

static void clearSummonOnParkIfAcaInactive(uint8_t gear) {
    // gear 0 (INVALID) et 7 (SNA) ignorés → transitions P↔R pendant Summon
    if (gear == 1 && !lastAca)
        clearSummonOnPark();
}

// ── CAN 280 : met à jour Parked + ACA + Summoning ─────────────
static void handle280(const uint8_t *data) {
    rx280++;
    last280Millis = (uint32_t)millis();
    uint8_t gear = readDIGear(data);
    int     gs   = gearState(gear);

    portENTER_CRITICAL(&stateMux);

    // Ne mettre à jour gateParked que si le gear est explicite (P, R, N, D)
    // gear INVALID(0) / SNA(7) pendant une transition → on conserve l'état précédent
    if (gs == 1)  gateParked = true;
    if (gs == 0)  gateParked = false;
    // gs == -1 → rien : on garde gateParked tel quel

    // DI_autonomyControlActive = data[6] bit 2 (bit 50)
    bool aca = (data[6] & 0x04) != 0;
    if (lastAca && !aca)
        sprSeen = false;   // fin d'épisode → reset SPR
    lastAca = aca;
    recomputeSummoning();

    clearSummonOnParkIfAcaInactive(gear);

    portEXIT_CRITICAL(&stateMux);
}

// ── CAN 390 : mise à jour Parked depuis DIF_gear (source secondaire) ─
// CAN 390 ne remplace CAN 280 que si le gear est explicite ET si
// CAN 280 n'a pas été reçu depuis plus de PARKED_TIMEOUT_MS
// (évite que 390 écrase un P confirmé par 280 avec un INVALID transitoire)
static void handle390(const uint8_t *data) {
    rx390++;
    uint8_t gear = readVehicleGear(data);
    int     gs   = gearState(gear);
    if (gs < 0) return;   // INVALID/SNA → ignorer complètement

    portENTER_CRITICAL(&stateMux);
    // N'écraser que si 280 est silencieux (fallback)
    uint32_t age = (uint32_t)millis() - last280Millis;
    if (last280Millis == 0 || age > PARKED_TIMEOUT_MS) {
        gateParked = (gs == 1);
        clearSummonOnParkIfAcaInactive(gear);
    }
    portEXIT_CRITICAL(&stateMux);
}

// ── CAN 921 : APActive ─────────────────────────────────────────
static void handle921(const uint8_t *data) {
    rx921++;
    bool ap = isDASActive(readDASStatus(data));

    portENTER_CRITICAL(&stateMux);
    gateAPActive = ap;
    portEXIT_CRITICAL(&stateMux);
}

// ── CAN 1016 : UI_selfParkRequest (SPR) ───────────────────────
// data[3] bits 4-7 : 0=NONE, 4=PRIME, 5=PAUSE, 7=FWD, 8=REV, 11=SMART
static void handle1016(const uint8_t *data, uint8_t dlc) {
    if (dlc < 4) return;
    rx1016++;
    uint8_t spr = (data[3] >> 4) & 0x0F;

    portENTER_CRITICAL(&stateMux);
    if (spr != 0)
        sprSeen = true;   // commande summon active → latch gate
    recomputeSummoning();
    portEXIT_CRITICAL(&stateMux);
}

// ═══════════════════════════════════════════════════════════════
// INJECTION SUMMON (CAN 1021, mux 1)
//
// Règle :
//   bit 19 → 0   Clears the summon EU restriction bit
//   bit 47 → 1   Sets the summon enable bit  (HW4)
//   bit 46 → 1   Sets the summon enable bit  (HW3)
//
// Condition : summonEnabled ET injectionGateOpen()
// ═══════════════════════════════════════════════════════════════

static void injectSummon(const twai_message_t &src) {
    // Lecture atomique de l'état
    bool en, gate;
    portENTER_CRITICAL(&stateMux);
    en   = summonEnabled;
    gate = injectionGateOpen();
    if (!gate) {
        // Mise à jour de la raison de blocage pour le dashboard
        if      (!gateAPActive  && !gateParked && !gateSummoning)
            strncpy(gateBlockReason, "AP-,Park-,Summon-", sizeof(gateBlockReason));
    }
    portEXIT_CRITICAL(&stateMux);

    if (!en || !gate)
        return;

    twai_message_t out;
    out.identifier       = src.identifier;
    out.data_length_code = src.data_length_code;
    out.flags            = 0;
    for (int i = 0; i < 8; i++) out.data[i] = src.data[i];

    // bit 19 = data[2] bit 3 → 0  (EU restriction)
    setBit(out.data, 19, false);
    // bit 47 = data[5] bit 7 → 1  (summon enable)
    setBit(out.data, 47, true);
    // HW3
    //setBit(out.data, 46, true); 
    rxMux1++;
    esp_err_t err = twai_transmit(&out, pdMS_TO_TICKS(2));
    if (err == ESP_OK) txOk++;
    else               txFail++;
}

// ═══════════════════════════════════════════════════════════════
// TÂCHE CAN (Core 1, priorité haute)
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
                    if (f.data_length_code >= 8 && readMuxID(f.data) == 1)
                        injectSummon(f);
                    break;
                default:
                    break;
            }
        }

        // Récupération bus-off automatique
        twai_status_info_t st;
        twai_get_status_info(&st);
        if (st.state == TWAI_STATE_BUS_OFF) {
            twai_initiate_recovery();
            vTaskDelay(pdMS_TO_TICKS(300));
        }

        // Watchdog : si CAN 280 silencieux depuis > PARKED_TIMEOUT_MS
        // (voiture garée avec Sentry, DI endormi) → forcer gateParked=true
        // pour ne pas bloquer le summon au réveil.
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
#define BLE_CHAR_CTRL_UUID "12345678-1234-1234-1234-123456789001"  // write
#define BLE_CHAR_STAT_UUID "12345678-1234-1234-1234-123456789002"  // notify

static BLECharacteristic *bleStatChar = nullptr;
static volatile bool      bleConnected = false;

// Callback connexion/déconnexion
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

// Callback écriture sur CTRL : "1" → enable, "0" → disable
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

    // Caractéristique CTRL (write)
    BLECharacteristic *ctrlChar = svc->createCharacteristic(
        BLE_CHAR_CTRL_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    ctrlChar->setCallbacks(new BleCtrlCb());

    // Caractéristique STATS (notify)
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

// Tâche BLE : envoie le JSON stats toutes les secondes si un client est connecté
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
    bool en, ap, parked, summon, aca, spr;
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

void loop() { vTaskDelay(pdMS_TO_TICKS(1000)); }
