# PumpPulse

**Low-cost agricultural pump-operation and water-delivery monitoring prototype**

PumpPulse is a proposed offline-first monitoring prototype built around an ESP32, a compatible water-flow sensor, and a 16×2 LCD. It estimates water flow and delivered volume and warns when flow remains below a configured threshold while the pump is *manually marked as running*.

> **Development status:** Prototype firmware / bench testing pending. This repository does not claim verified field performance, water savings, or novelty. Confirm the exact sensor model and electrical interface before wiring.

## Features in this prototype
- Pulse-based flow-rate estimate and cumulative volume estimate.
- 16×2 I²C LCD status display.
- Manual pump-state switch input.
- Startup grace period and persistent low-flow warning.
- Local ESP32 Wi-Fi access point and phone-friendly dashboard.
- Serial output for debugging.

## Important limitations
- Pump ON/OFF is entered manually using a switch; the system does not independently confirm motor operation.
- Low/no flow is a warning condition, not a diagnosis of the cause.
- Flow calibration and threshold values are placeholders until calibrated against the actual sensor and test rig.
- Event counters and volume totals are stored in RAM and reset after a restart.
- This is monitoring-only. It does not control a pump, replace dry-run/overload protection, or send remote alerts.
- The current firmware is intended for a pulse-output sensor. Analog, RS485, or other sensor interfaces require different code and wiring.

## Hardware
- ESP32 development board
- Pulse-output water-flow sensor compatible with the ESP32 input interface
- 16×2 LCD with I²C adapter (or adjust the display code for your LCD)
- Toggle switch between GPIO 26 and GND
- Low-voltage DC pump, suitable rated supply, reservoir and tubing
- Breadboard/jumpers; optional buzzer/LED

## Provisional pin map
| Function | ESP32 pin |
|---|---|
| Flow sensor pulse signal | GPIO 27 |
| Manual pump-state switch | GPIO 26 (switch to GND means ON) |
| LCD SDA | GPIO 21 |
| LCD SCL | GPIO 22 |

**Electrical checks are mandatory:** ESP32 GPIO is 3.3 V logic and is not 5 V tolerant. Check the flow sensor output and LCD I²C pull-ups. Use suitable level shifting or signal conditioning if required. Do not assume sensor VCC is 3.3 V or 5 V without checking its datasheet.

## Arduino IDE setup
1. Install Arduino IDE.
2. In Boards Manager, install **esp32 by Espressif Systems**.
3. Install a compatible **LiquidCrystal_I2C** library (the sketch uses `LiquidCrystal_I2C` API).
4. Select the correct ESP32 board and COM port.
5. Open `firmware/PumpPulse_ESP32/PumpPulse_ESP32.ino`.
6. Confirm the LCD I²C address (commonly `0x27`, but not universal).
7. Verify the flow sensor output type, pinout and voltage before connecting it.
8. Upload with the pump power disconnected first; check serial output at 115200 baud.

## Local dashboard
After boot, connect a phone to Wi-Fi:
- SSID: `PumpPulse`
- Password: `PumpPulse26`
- Open: `http://192.168.4.1`

The access point is local-only; internet access is not required. A phone may report “no internet”—that is expected.

## Calibration
1. Route water through the sensor into a graduated container.
2. Collect a known volume and count pulses.
3. Calculate: `pulses_per_litre = counted_pulses / measured_litres`.
4. Repeat at least three times and use a repeatable value.
5. Compare flow estimate with timed collection: `flow_L_min = 60 × volume_L / time_seconds`.
6. Set the low-flow threshold based on the actual test rig. Do not use a small sensor outside its rated flow, pressure, temperature or pipe range.

## Fault logic
- Pump switch OFF: status `PUMP OFF`.
- Pump switch ON: startup grace period.
- After grace period, flow below threshold begins a persistence timer.
- If low flow persists beyond the configured delay, status becomes `NO/LOW FLOW`.
- If flow returns above threshold, the low-flow timer and active warning clear.

The absence of pulses alone cannot reliably distinguish a dry source, closed valve, blockage, sensor failure, or other cause.

## Testing
See [`docs/architecture.md`](docs/architecture.md), [`tests/test_cases.csv`](tests/test_cases.csv), and [`data/sample_test_log.csv`](data/sample_test_log.csv). The sample log is a blank template, not experimental evidence.

## Safety
- Use only a low-voltage DC pump for bench tests.
- Keep water away from the ESP32, laptop, connectors and power supplies.
- Never power a pump from an ESP32 GPIO pin.
- Do not connect this prototype to agricultural mains wiring, motor terminals, contactors or starter panels.
- Do not disable or replace existing pump protections.
- Do not intentionally run the pump dry unless its manufacturer explicitly permits it.
- Any future mains-side measurement or field installation must use appropriately rated isolated equipment and a qualified electrician.

## Roadmap
- Confirm exact sensor model and electrical interface.
- Calibrate and record bench results.
- Add sensor-disconnection plausibility checks.
- Add persistent event history after the basic system is validated.
- Consider independent pump-state sensing only after a safe interface is designed.
- Conduct farmer interviews and field observation before claiming impact.

## License
MIT. See [`LICENSE`](LICENSE).
