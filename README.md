
# EU Summon Unlock V2.0

> ⚠️ Research / educational firmware only.  
> ESP32 firmware that lifts the EU geographic restriction on Smart Summon & autopilot.   
> This project interacts with a vehicle CAN bus. It is intended for controlled bench testing, code review, and research environments only.It sends signals directly to the controller. Do not use this on public roads or in any situation where unsafe behavior could put people or property at risk. You are responsible for your own testing, wiring, configuration, and local laws.
--- 


## What V2 Update Changes
- bypass R79 EU restriction in Autopilot.
- expanded lateral acceleration limits
- lane changes near forks isn't disabled (EAP)
- instantaneous lane change on blinker (EAP)
- no lane change timeout once initiated (EAP)
- takes forks and exits automatically (EAP)

## Future Update
- redesign BLE dashboard
- code cleaning
- unified version Lilygo-t2can

## How it works

Injection only occurs when at least one of the following conditions is true:

```
gate = Parked OR Summoning
force = Autopilot active 
```

| Flag | Source | Condition |
|------|--------|-----------|
| `Parked` | CAN 280 / 390 | Gear == P |
| `Summoning` | CAN 280 + 1016 | `ACA == 1` AND `SPR ≠ 0` |
  | `gateAPActive ` | CAN 921   | status == 3 ,4 ,5 ,6 |

This prevents any injection while driving manually.


## Compatibility
- Confirmed working on 2024 Model Y HW4 firmware 2026.26

| Hardware | Tested | CAN ID | Mux | Bit 19 | Bit 47 |
|----------|--------|--------|-----|--------|--------|
| HW4      | ✓      | 1021   | 1   | ✓     | ✓      |

> The current firmware targets **bit 47** per the confirmed rule set. edit summon_unlock.ino setBit if needed.

---

## Hardware

- **ESP32** (any variant with TWAI/CAN peripheral)
- CAN transceiver (e.g. SN65HVD230, MCP2562)
- Wired inline on the chassis CAN bus — typically at the OBD-II port or X179 connector (tested on model Y HW4 Pin 13 & 14)

### Pin defaults (Atom S3 lite with can base)

```cpp
#define CAN_TX_PIN  5
#define CAN_RX_PIN  6
```
Change these in `summon_unlock.ino` to match your wiring.

---

## Web Bluetooth Dashboard
A standalone dashboard page hosted on GitHub Pages allows you to control and monitor the device directly from Chrome on Android — without connecting to the ESP32 Wi-Fi network.

How it works
The ESP32 exposes a BLE GATT server alongside the existing Wi-Fi AP. Both run simultaneously on separate RTOS tasks. The GitHub Pages app uses the Web Bluetooth API built into Chrome to communicate directly with the ESP32 over BLE.

Usage:

1- Open the GitHub Pages URL https://06066060606060.github.io/Summon-Unlock/ in Chrome on Android
2- Tap Connect via Bluetooth
3- Select SummonUnlock from the device picker
4- Use the Enable / Disable buttons to control injection
5- All gate flags and CAN counters update in real time via BLE notify
- The BLE device name is always SummonUnlock regardless of MAC address, making it easy to identify in the picker

---  
## Wi-Fi Dashboard

After boot the ESP32 creates a Wi-Fi access point:

| Parameter | Value |
|-----------|-------|
| SSID | `SummonUnlock-XXYY` (last 2 bytes of MAC) |
| Password | `summon1234` |
| Dashboard | [http://192.168.4.1](http://192.168.4.1) |
  
  ---  
### Dashboard panels

**Summon Unlock** — master enable/disable toggle, persisted to NVS across reboots.

**Injection Gate** — real-time state of `Parked` and `Summoning` flags.  
`APActive` (from CAN 921).

**CAN Frames** — per-ID receive counters (280, 390, 921, 1016, 1021 mux1), TX ok/fail, bus state, uptime.

### REST API

```
GET  /api/stats    → JSON snapshot of all state
POST /api/enable   → enable injection, persist to NVS
POST /api/disable  → disable injection, persist to NVS
```

---

## CAN frames monitored

| ID (dec) | ID (hex) | Signal | Used for |
|----------|----------|--------|----------|
| 280 | 0x118 | `DI_systemStatus` | Gear (Parked), ACA |
| 390 | 0x186 | `DIF_torqueStatus` | Gear backup (Parked) |
| 921 | 0x399 | `DAS_autopilotStatus` | APActive (info) |
| 1016 | 0x3F8 | `UI_driverAssistControl` | SPR (summon command) |
| 1021 | 0x3FD | `UI_autopilotControl` | **Injection target** (mux 1) |

---

## Flash using arduino IDE
If you encounter a compilation error (Sketch too big), change the partition Scheme with 2MB SPIFFS.
- Open Tools > Partition Scheme.
- Pick the layout that matches your project needs.
- Recompile and upload the sketch
---  
## Variant
-  EU Summon Unlock V1 with serial can logging
https://github.com/06066060606060/Summon-Unlock/tree/can-log-serial
-  Nag-killer & EU-Summon-Unlock V1 for LilyGO/T-2Can
https://github.com/06066060606060/nag-killer/tree/t2can-test

---

## Safety notes

- The firmware only modifies **one frame, two bits**. It does not alter speed limits, steering, braking, or any safety-critical signal.
- The injection gate ensures the modification is active **only when parked or during an active autopilot session** — never during normal driving.
- Disable via the dashboard at any time; state is persisted to NVS.
- **Use at your own risk. This project is for research and educational purposes only.**

---

<img width="230" height="551" alt="summon" src="https://github.com/user-attachments/assets/d8c8b306-8155-48e1-bacd-6af79aef1c88" />

## Credits

- `Ev Open Can Mod` https://github.com/ev-open-can-tools/ev-open-can-tools
- Created by X₿mod.
- ESP32 TWAI driver by Espressif Systems
- Automotive CAN research community


## Discord server: 
https://discord.gg/9t5pMuts3
