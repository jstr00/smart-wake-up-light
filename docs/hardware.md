# Hardware

The circuit uses two low-side N-MOSFET channels to switch a common-positive 24 V warm-/cold-white LED strip. The ESP32-H2 and MOSFET sources must share a ground reference. The two 10 kΩ gate pull-down resistors connect to GND.

## Components

| Qty | Component |
|---:|---|
| 1 | ESP32-H2 DevKitM-1 |
| 1 | KY-040 rotary encoder |
| 2 | IRLZ44NPBF N-MOSFET, each with a gate resistor and 10 kΩ gate pull-down |
| 1 | MP1584EN buck converter module |
| 1 | BTF-LIGHTING 24 V FCOB CCT COB strip, 640 LEDs/m, 1 m |
| 1 | CJ-2403 DC supply, labeled 24 V / 3 A / 72 W |
| As required | Rated connectors, cabling and DC over-current protection |

## Pin mapping

| Function | GPIO |
|---|---:|
| Cold white (CW) | 4 |
| Warm white (WW) | 5 |
| Status LED (WS2812) | 8 |
| Encoder CLK / DT / SW | 10 / 11 / 12 |

The MOSFET specification does **not** guarantee a particular on-resistance at 3.3 V gate drive. Verify gate voltages, current, MOSFET and converter temperature, wiring and over-current protection for the actual LED strip. Prototype input assumptions and power/PWM calculations are in [PWM and power calculations](pwm-and-power.md).

Each builder must verify component ratings, wiring, isolation, over-current protection and thermal suitability for their own assembly. No electrical safety certification is claimed.