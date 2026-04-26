# Mandalorian Jetpack Controller — Pin Mapping & Netlist
**Firmware:** `jetpack_controller_final.ino` v2.2  
**MCU:** ESP32 Dev Board  
**Date:** 2026-04-25

---

## Component List

| Ref | Component | Notes |
|-----|-----------|-------|
| U1  | ESP32 38-pin Terminal Breakout Board | WiFi AP + HTTP server; **PCB: 63 × 66 mm** (confirmed via Amazon product listing), mounting holes **57.75 × 60.75 mm** (M3, caliper confirmed + test print adjusted) |
| U2  | L298N H-Bridge Motor Driver | Dual pump control; **measured PCB: 43.5 × 43.5 mm**, mounting holes **36.75 × 36.75 mm** (M3, caliper confirmed + test print adjusted), **heatsink height: 29 mm minimum clearance** |
| LED1 | NeoPixel Ring — 16 LED | GPIO 21, NEO_GRB 800kHz |
| M1  | Water Pump 1 | DC motor, ~12V |
| M2  | Water Pump 2 | DC motor, ~12V |
| PS1 | 4× AA Battery Pack (6V total) | Powers pumps via L298N |
| PS2 | 5V supply (or L298N onboard 5V reg) | Powers NeoPixel ring |

---

## ESP32 Pin Assignments

| GPIO | Direction | Connected To | Function |
|------|-----------|--------------|----------|
| 23   | OUTPUT    | L298N IN1    | Pump 1 — forward drive (active HIGH) |
| 22   | OUTPUT    | *(not connected)* | Pump 1 — reverse/brake; **not currently used**, could be wired to L298N IN2 for future reverse control |
| 19   | OUTPUT    | L298N IN3    | Pump 2 — forward drive (active HIGH) |
| 18   | OUTPUT    | *(not connected)* | Pump 2 — reverse/brake; **not currently used**, could be wired to L298N IN4 for future reverse control |
| 21   | OUTPUT    | NeoPixel DIN | LED ring data |
| VIN  | PWR IN    | L298N 5V reg output | Main power in — accepts 5–12V, onboard ESP32 regulator steps down to 3.3V |
| GND  | PWR       | L298N GND, NeoPixel GND, Battery GND | Common ground (REQUIRED) |

> **Note:** GPIO 22 (IN2) and GPIO 18 (IN4) are driven LOW in firmware but are  
> **not physically connected** in the current build. The L298N's internal pull-downs  
> hold these LOW automatically. They could be wired in future for reverse/brake control.

---

## L298N H-Bridge Connections

### Motor A — Pump 1

| L298N Pin | Connected To | Notes |
|-----------|--------------|-------|
| IN1       | ESP32 GPIO 23 | Direction control — HIGH = forward |
| IN2       | *(not connected)* | Reverse/brake — **not currently used**; L298N internal pull-down holds LOW |
| ENA       | Jumper ON (5V) | Always enabled (no PWM speed control) |
| OUT1      | Pump 1 wire (+) | Motor output A+ |
| OUT2      | Pump 1 wire (−) | Motor output A− |

### Motor B — Pump 2

| L298N Pin | Connected To | Notes |
|-----------|--------------|-------|
| IN3       | ESP32 GPIO 19 | Direction control — HIGH = forward |
| IN4       | *(not connected)* | Reverse/brake — **not currently used**; L298N internal pull-down holds LOW |
| ENB       | Jumper ON (5V) | Always enabled (no PWM speed control) |
| OUT3      | Pump 2 wire (+) | Motor output B+ |
| OUT4      | Pump 2 wire (−) | Motor output B− |

### L298N Power

| L298N Pin | Connected To | Notes |
|-----------|--------------|-------|
| +12V (VS) | PS1 +6V (4× AA) | Motor supply voltage |
| GND       | PS1 GND + ESP32 GND | Common ground — MUST be tied together |

---

## NeoPixel Ring (16 LED)

| NeoPixel Pin | Connected To | Notes |
|--------------|--------------|-------|
| DIN (Data)   | ESP32 GPIO 21 | Signal — add 300–500Ω series resistor recommended |
| VCC (+5V)    | External 5V  | Do NOT power from ESP32 3.3V; needs 5V |
| GND          | Common GND   | Tied to ESP32 GND and PS1 GND |

> **Tip:** The L298N's onboard 5V regulator output can supply both the NeoPixel  
> ring and the ESP32 from the 6V battery pack. Check current capacity — 16 LEDs  
> at full white can draw ~960mA. At the brightness levels in this firmware  
> (BASE=60, MAX=160 out of 255), actual draw is roughly 150–300mA for the ring.

---

## Power Architecture

```
4× AA Battery Pack (+6V) ────────────── L298N VS (motor supply)
                                              │
                                         L298N OUT1/OUT2 ──── Pump 1
                                         L298N OUT3/OUT4 ──── Pump 2
                                              │
                                   L298N onboard 5V reg ───── ESP32 VIN (5V)
                                                        └──── NeoPixel VCC (5V)

Common GND ──── Battery pack GND
           ──── L298N GND
           ──── ESP32 GND
           ──── NeoPixel GND
```

---

## Logic State Table — Pump Control

| State     | GPIO 23 (IN1) | GPIO 22 (IN2) | GPIO 19 (IN3) | GPIO 18 (IN4) | Result |
|-----------|:---:|:---:|:---:|:---:|--------|
| IDLE/STOP | LOW | LOW* | LOW | LOW* | Pumps OFF (coast) |
| RUNNING   | HIGH| LOW* | HIGH| LOW* | Both pumps FORWARD |

*GPIO 22 and GPIO 18 are driven LOW by firmware but **not physically connected** in current build.

---

## WiFi / Network

| Parameter | Value |
|-----------|-------|
| Mode      | Access Point (AP) |
| SSID      | `MandalorianJetpack` |
| Password  | `thisIsTheWay` |
| IP Address| `192.168.4.1` |
| Port      | 80 (HTTP) |

### HTTP API

| Endpoint   | Method | Description |
|------------|--------|-------------|
| `/`        | GET    | Full test page with activate button |
| `/popup`   | GET    | Minimal PWA popup page (iPhone app) |
| `/activate`| GET    | Trigger jetpack sequence — also fires `Mando_rocket_sequence.mp3` on iPhone via postMessage |
| `/status`  | GET    | Returns `{"state":"idle"}` or `{"state":"active"}` |
| `/stop`    | GET    | Emergency stop — pumps off, LEDs off |

---

## Sequence Timing Summary

| Phase | Duration | Description |
|-------|----------|-------------|
| Pump ON | 5 sec | Both pumps run from activation |
| LED start delay | +2 sec after pump start | NeoPixel sequence begins |
| Ignition | 5 sec | Spark attempts → flame catch |
| Full Thrust | 33 sec | Main flame burn with breathing + bursts |
| Shutdown | 16 sec | Fade, sputter, blackouts |
| Off | 3 sec | All LEDs off, system returns to IDLE |
| **Total LED** | **57 sec** | Ignition + Full Thrust + Shutdown + Off |
