# PumpPulse Technical Design

## Logical architecture

```text
Reservoir -> Low-voltage DC pump -> Flow sensor -> Return / measuring container
                                     |
                                     | pulse output (verified interface)
                                     v
Manual pump-state switch ----------> ESP32
                                      |-- 16x2 LCD (I2C)
                                      |-- local Wi-Fi access point
                                      |     `-- phone browser dashboard
                                      `-- serial debug output
```

## State model

1. `PUMP OFF`: manual switch is open.
2. `STARTING`: switch becomes ON; allow a startup grace period.
3. `LOW FLOW`: flow remains below configured threshold; persistence timer runs.
4. `NO/LOW FLOW`: low flow persists beyond the delay.
5. `NORMAL`: flow exceeds the threshold; clear the active low-flow timer.

## Measurements
- `pulsesPerLiter`: calibrate against a measured volume.
- `flowLpm = pulses_in_window × 60000 / (pulsesPerLiter × elapsed_ms)`.
- `totalLitres = accumulated_pulses / pulsesPerLiter`.

## Safety boundaries
The prototype is a monitoring demonstrator only. It must not be wired into mains motor terminals or used as a substitute for certified protection. The flow sensor's hydraulic and electrical ratings must fit the actual installation.
