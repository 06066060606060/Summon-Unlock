
# EU Summon Unlock V2.5

> ⚠️ Research / educational firmware only.  
> ESP32 firmware that lifts the EU geographic restriction on Smart Summon & autopilot.   
> This project interacts with a Tesla vehicle CAN bus. It is intended for controlled bench testing, code review, and research environments only.It sends signals directly to the controller. Do not use this on public roads or in any situation where unsafe behavior could put people or property at risk. You are responsible for your own testing, wiring, configuration, and local laws.
--- 
## What V2.5 Update Changes
- Added TLSSC Restore for banned car

--------------------Update 2.4-------------------  
- Fixes an error that injects TLSSC even when disabled
- 
--------------------Update 2.3-------------------  
- Added a toggle in dashboard to enable TLSSC where it is not available.
- (you need a valid EAP/FSD subscription)  
- bypass R79 EU restriction in AP
- Expend summon to +/-  85m
- expanded lateral acceleration limits
- lane changes near forks isn't disabled (EAP)
- instantaneous lane change on blinker (EAP)
- no lane change timeout once initiated (EAP)
- takes forks and exits automatically (EAP)
- Continue on Green with Car in Front (EAP)
- OTA Update

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

⚠️ Do not enable TLSSC if you do not have the EAP option.

## Compatibility
- AP Injection doesn't work before 2026.20
- Confirmed working with firmware 2026.26.1
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

## Build firmware using arduino IDE for OTA
- Open Sketch > Export Compiled Binary.
- Open /summon_unlock/build/ folder
- Upload summon_unlock.ino.bin (900Ko) using the web dashboard & Update
---  

## Variant
- EU Summon Unlock V2.3 TLSSC restore for banned car
 https://github.com/06066060606060/Summon-Unlock/tree/ban-version

-  Nag-killer & EU-Summon-Unlock unified for LilyGO/T-2Can  
https://github.com/06066060606060/T2CAN-Nag-killer-EU-unlock

---

## Safety notes

- The firmware only modifies **one frame, two bits**. It does not alter speed limits, steering, braking, or any safety-critical signal.
- The injection gate ensures the modification is active **only when parked or during an active autopilot session** — never during normal driving.
- Disable via the dashboard at any time; state is persisted to NVS.
- **Use at your own risk. This project is for research and educational purposes only.**

---

## Discord server: 
https://discord.gg/euPbYG8Npc

> **Support the project:**  
> [![Buy Me A Coffee](https://www.buymeacoffee.com/assets/img/custom_images/orange_img.png)](https://buymeacoffee.com/mickymurcid)  

Bitcoin: bc1pl9nuyhqd78gjc2wdcqr39de7qwtff732ngr28vy8r2sxfa7a6uzsrhe387  
Lightning: ₿cakegrip53@phoenixwallet.me



## Credits

- Inspired by `Ev Open Can Mod` https://github.com/ev-open-can-tools/ev-open-can-tools
- Created by X₿mod.
- ESP32 TWAI driver by Espressif Systems
- Automotive CAN research community

<img width="230" height="551" alt="summon" src="https://github.com/user-attachments/assets/d8c8b306-8155-48e1-bacd-6af79aef1c88" />

