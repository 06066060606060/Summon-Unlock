# Summon EU Unlock 
inspired by nicolozak Nag-killer & ev-open-can-tools

> **Research / educational project — not for use on public roads.**

ESP32 firmware that lifts the EU geographic restriction on Smart Summon by intercepting and modifying a single CAN frame on the vehicle bus.

---

## How it works

Injection only occurs when at least one of the following conditions is true:

```
gate = Parked OR Summoning
```

| Flag | Source | Condition |
|------|--------|-----------|
| `Parked` | CAN 280 / 390 | Gear == P |
| `Summoning` | CAN 280 + 1016 | `ACA == 1` AND `SPR ≠ 0` |

This prevents any injection while driving manually or under plain AP/TACC.

### Summon vs TACC discrimination

`DI_autonomyControlActive` (ACA, CAN 280 bit 50) is active during AP, TACC **and** Smart Summon — it cannot be used alone.  
`UI_selfParkRequest` (SPR, CAN 1016 data[3] bits 4–7) is non-zero only when a Summon command has been issued.

```
Summoning = lastACA && sprSeen
```

- TACC only → `ACA=1`, `SPR=0` → gate **closed**, no injection
- Smart Summon → `ACA=1`, `SPR≠0` → gate **open**, injection active
- ACA falling edge → `sprSeen` cleared (episode reset)
- Gear P + ACA=0 → full reset

---

## Compatibility

| Hardware | Tested | CAN ID | Mux | Bit 19 | Bit 47 |
|----------|--------|--------|-----|--------|--------|
| HW4 (FSD Computer) | — | 1021 | 1 | ✓ | ✓ |

> Bit 46 is used on some older HW3 builds. The current firmware targets **bit 47** per the confirmed rule set.

---

## Hardware

- **ESP32** (any variant with TWAI/CAN peripheral)
- CAN transceiver (e.g. SN65HVD230, MCP2562)
- Wired inline on the vehicle CAN bus — typically at the OBD-II port or X179 connector (tested on model Y HW4 Pin 13 & 14)

```
Vehicle CAN bus
    ├── CAN H ──┬── [Transceiver] ── ESP32 TX/RX
    └── CAN L ──┘
```

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
1-Open the GitHub Pages URL https://06066060606060.github.io/Summon-Unlock/ in Chrome on Android
2-Tap Connect via Bluetooth
3-Select SummonUnlock from the device picker
4-Use the Enable / Disable buttons to control injection
5-All gate flags and CAN counters update in real time via BLE notify

-The BLE device name is always SummonUnlock regardless of MAC address, making it easy to identify in the picker


## Wi-Fi Dashboard

After boot the ESP32 creates a Wi-Fi access point:

| Parameter | Value |
|-----------|-------|
| SSID | `SummonUnlock-XXYY` (last 2 bytes of MAC) |
| Password | `summon1234` |
| Dashboard | [http://192.168.4.1](http://192.168.4.1) |

### Dashboard panels

**Summon Unlock** — master enable/disable toggle, persisted to NVS across reboots.

**Injection Gate** — real-time state of `Parked` and `Summoning` flags.  
`APActive` (from CAN 921) is shown as info only — it does not open the gate.

**Summon / TACC discrimination** — live view of `ACA` and `SPR` signals used to distinguish Smart Summon from plain TACC.

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

## Serial output

```
IDF: v5.x.x
twai_install=0  twai_start=0
=== SummonUnlock ready ===
  Injection gate : Parked || Summoning
  CAN 1021 mux1  : bit19->0, bit47->1
  summonEnabled  : true
[WIFI] SSID=SummonUnlock-AABB  PASS=summon1234  IP=192.168.4.1
```

---

## Project structure

```
summon_unlock/
├── summon_unlock.ino   # Main logic — TWAI, gate, injection, Wi-Fi
└── index_html.ino      # Dashboard HTML served from PROGMEM
```

---

## Safety notes

- The firmware only modifies **one frame, two bits**. It does not alter speed limits, steering, braking, or any safety-critical signal.
- The injection gate ensures the modification is active **only when parked or during an active Summon session** — never during normal driving.
- Disable via the dashboard at any time; state is persisted to NVS.
- **Use at your own risk. This project is for research and educational purposes only.**

---


