# EU Summon Unlock V2.6.0

### ESP32 firmware for Tesla Smart Summon & Autopilot EU restriction research

> ⚠️ **Research / educational firmware only**
>
> This project interacts with a Tesla vehicle CAN bus. It is intended for controlled bench testing, code review, and research environments only.
>
> It sends signals directly to the controller. **Do not use this on public roads or in any situation where unsafe behavior could put people or property at risk.**
>
> You are responsible for your own testing, wiring, configuration, and compliance with local laws.

---

### ⚠️ DO NOT ACTIVATE TLSSC RESTORE ON NON BANNED CAR (you will be banned instantly)

## 📋 What's New

### V2.6.0
- Includes an updated summon logic for much better stability. (500ms echo)


## 🇪🇺 EU Unlock

- Bypass **R79 EU restriction** in AP.
- Expand Summon range to **±85 m**.
- Expanded lateral acceleration limits.
- Lane changes near forks are not disabled (EAP).
- Instantaneous lane change on blinker (EAP).
- No lane-change timeout once initiated (EAP).
- Automatically takes forks and exits (EAP).
- blindspot aggressiveness settings during lane change (madmax)
- Toggle to activate **TLSSC** where it is not available.
  - A valid **EAP/FSD subscription** is required.
- Continue on Green with Car in Front (**TLSSC**).
- **TLSSC Restore** for banned cars. 
- OTA update support.

---

## ⚙️ How It Works

Injection only occurs when at least one of the following conditions is true:

```text
gate  = Parked, Summoning or Autopilot active
```
This prevents injection while driving manually.



---

## ✅ Compatibility

| Item | Version / Status |
|---|---|
| AP Injection | Does not work before **2026.20** |
| Confirmed working | **2026.26.1** |

---

## 🔧 Hardware

### Required Hardware

- **ESP32** — any variant with a TWAI/CAN peripheral.
- **CAN transceiver** — e.g. SN65HVD230 or MCP2562.
- Wired inline on the **chassis CAN bus**.
  - Typically at the OBD-II port or X179 connector.
  - Tested on **Model Y HW4, pins 13 & 14**.

### Pin Defaults — Atom S3 Lite with CAN Base

```cpp
#define CAN_TX_PIN  5
#define CAN_RX_PIN  6
```

Change these definitions in `summon_unlock.ino` to match your device.

---

## 🌐 Wi-Fi Dashboard

After boot, the ESP32 creates a Wi-Fi access point.

| Parameter | Value |
|---|---|
| **SSID** | `SummonUnlock-XXYY` — last 2 bytes of MAC |
| **Password** | `summon1234` |
| **Dashboard** | `http://192.168.4.1` |

### Dashboard Panels

#### Summon Unlock

Master enable/disable toggle, persisted to **NVS** across reboots.

#### Injection Gate

Real-time state of:

- `Parked`
- `Summoning`
- `APActive` — from CAN 921

#### CAN Frames

Displays:

- Per-ID receive counters:
  - `280`
  - `390`
  - `921`
  - `1016`
  - `1021 mux1`
- TX OK / fail counters.
- CAN bus state.
- Uptime.

---

## 🔌 REST API

```text
GET  /api/stats    → JSON snapshot of all state
POST /api/enable   → Enable injection and persist to NVS
POST /api/disable  → Disable injection and persist to NVS
```

---

## 📡 CAN Frames Monitored

| ID (dec) | ID (hex) | Signal | Used for |
|---:|---:|---|---|
| 280 | `0x118` | `DI_systemStatus` | Gear (Parked), ACA |
| 390 | `0x186` | `DIF_torqueStatus` | Gear backup (Parked) |
| 921 | `0x399` | `DAS_autopilotStatus` | APActive (info) |
| 1016 | `0x3F8` | `UI_driverAssistControl` | SPR (Summon command) |
| 1021 | `0x3FD` | `UI_autopilotControl` | **Injection target** (mux 1) |

---

## 📦 Build Firmware for OTA
For the 1st flash with arduino IDE:  
Partition Scheme:
Default 4MB with spiffs(1.2MB APP/1.5MB SPIFFS)

Build the firmware using **Arduino IDE**:

1. Open the sketch in Arduino IDE.
2. Select:
   **Sketch → Export Compiled Binary**
3. Open:
   ```text
   /summon_unlock/build/
   ```
4. Locate:
   ```text
   summon_unlock.ino.bin
   ```
   *(approximately 900 KB)*
5. Open the web dashboard.
6. Go to **Update** and upload the `.bin` file.

---

## 🔀 Variant

### Nag-killer & EU-Summon-Unlock — Unified LilyGO / T-2Can

Repository:

https://github.com/06066060606060/T2CAN-Nag-killer-EU-unlock

### Advanced EAP & EU-Summon-Unlock — LilyGO / T-2Can

Repository:

https://github.com/06066060606060/Advanced-eap-eu-unlock

---

## ⚠️ Safety Notes

- The firmware only modifies **one frame and two bits**.
- It does not alter speed limits, steering, braking, or other safety-critical signals.
- The injection gate ensures the modification is active **only when parked or during an active autopilot session**, and not during normal manual driving.
- Injection can be disabled via the dashboard at any time.
- The enabled/disabled state is persisted to **NVS**.
- **Use at your own risk. This project is for research and educational purposes only.**

---

## 💬 Community & Support

### Discord

Join the project Discord server:

https://discord.gg/euPbYG8Npc

---


### ☕ Support the Project

<a href="https://www.buymeacoffee.com/xbmod" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me a Coffee" style="height: 60px !important;width: 217px !important;" ></a>

### ₿ Bitcoin

```text
bc1pl9nuyhqd78gjc2wdcqr39de7qwtff732ngr28vy8r2sxfa7a6uzsrhe387
```

### ⚡ Lightning

```text
₿cakegrip53@phoenixwallet.me
```

---

## 🙏 Credits

- Inspired by **Ev Open Can Tools**  
- Created by **X₿mod**.
- ESP32 TWAI driver by **Espressif Systems**.
- Automotive CAN research community.

---

## 📸 Dashboard

<img width="230" height="551" alt="Summon Unlock Dashboard" src="https://github.com/user-attachments/assets/d8c8b306-8155-48e1-bacd-6af79aef1c88" />
