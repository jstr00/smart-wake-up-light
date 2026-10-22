# Test matrix

IDs are stable identifiers shared by the test names and this table. Each test function starts with its ID in lowercase. Parameterized pytest cases use the same ID with a descriptive parameter suffix.

## Unit tests (Twister/Ztest, `native_sim`)

| ID | Why | What and how | Expected result |
|---|---|---|---|
| `ut_light_001` | Avoid invalid startup brightness | Initialize with 0 and 255 | OFF, brightness clamped to 0..100 |
| `ut_light_002` | Make local on/off predictable | Toggle OFF → NIGHTLIGHT → OFF → NIGHTLIGHT | Correct modes, stored brightness retained |
| `ut_light_003` | Button must interrupt wake-up | Start wake-up, toggle, read output | OFF; WW and CW both zero |
| `ut_light_004` | Rotation should interrupt animation | Adjust during wake-up and beyond boundaries | NIGHTLIGHT, brightness clamped to 0..100 |
| `ut_light_005` | Match nightlight brightness to PWM | Read OFF and NIGHTLIGHT duty at 10% and 100% | OFF is zero; WW is 1% or 100% duty; CW stays zero |
| `ut_light_006` | Preserve warm-first sequence | Sample wake-up at 0, 250, 499 and 500‰ | WW ramps quadratically to 100%; CW stays off |
| `ut_light_007` | Reach full combined output | Sample wake-up at 500, 750, 1000 and beyond | WW stays at 100%; CW increases to 100% and clamps |
| `ut_light_008` | Avoid brightness regression | Sweep all 1001 progress values | Both PWM duties monotonic and bounded |
| `ut_coap_001` | Separate health check from commands | Parse exact `ping` and near misses | Only exact `ping` accepted; no duration emitted |
| `ut_coap_002` | Enforce configured duration bounds | Parse 0, 1, 30, 3600, 3601 and empty | Only 1..3600 valid for a 3600-second limit |
| `ut_coap_003` | Reject malformed user input | Parse signs, spaces, decimal point and letters | All rejected |
| `ut_coap_004` | Prevent integer wraparound | Parse values beyond 32-bit unsigned range | All rejected |
| `ut_coap_005` | Avoid invalid buffer access/data | Parse null pointers and an embedded NUL | All rejected; output unchanged |
| `ut_coap_006` | Verify configurable validation | Parse against maximum 0, 1 and `UINT32_MAX` | Only values inside specified limit accepted |
| `ut_encoder_001` | Count clockwise rotation | Feed a complete clockwise quadrature sequence | Exactly +1 detent |
| `ut_encoder_002` | Count counterclockwise rotation | Feed a complete reverse sequence | Exactly −1 detent |
| `ut_encoder_003` | Reject impossible signal edges | Insert a two-bit jump then a valid cycle | Accumulator resets; valid cycle gives +1 |
| `ut_encoder_004` | Resist switch bounce | Feed forward/backward edge chatter then full turn | One detent, no extra counts |
| `ut_encoder_005` | Avoid false counts on reversal | Feed incomplete forward turn then reverse | Zero detents; accumulator reset |
| `ut_encoder_006` | Ignore unchanged GPIO levels | Repeat same state 1000 times | Zero detents |
| `ut_encoder_007` | Stress decoder arithmetic | Feed 10,000 complete clockwise cycles | Exactly 10,000 detents |

These tests exercise portable logic, **not** encoder interrupt timing, Zephyr queue capacity, actual PWM hardware, or Thread routing.

## Service-discovery helper tests (pytest, no hardware)

| ID | Why | What and how | Expected result |
|---|---|---|---|
| `ut_discovery_001` | Build a correct endpoint without a hardcoded address | Pass synthetic DNS-SD SRV/TXT/AAAA data | Valid IPv6 CoAP URI with advertised port/path |
| `ut_discovery_002` | Avoid unusable link-local targets | Supply only `fe80::` | Clear rejection |
| `ut_discovery_003` | Avoid malformed/wrong services | Supply invalid protocol, resource path, port or missing AAAA | Clear rejection for every case |

## Live integration tests (pytest, real Thread device)

| ID | Why | What and how | Expected result |
|---|---|---|---|
| `it_discovery_001` | Ensure service is visible from the LAN | Browse `_smart-wakeup._udp.local.`, resolve SRV/TXT/AAAA | One selected instance yields a valid CoAP endpoint |
| `it_coap_001` | Check remote reachability | Send Confirmable POST `ping` | CoAP `2.04 Changed` |
| `it_coap_002` | Check both supported write methods | Send Confirmable PUT `ping` | CoAP `2.04 Changed` |
| `it_coap_003` | Reject malformed input over the network | POST empty, zero, signed, decimal, text, whitespace, NUL and oversized payloads | Every request returns `4.00 Bad Request` |
| `it_coap_004` | Respect resource method contract | Send Confirmable GET `/wakeup` | CoAP `4.05 Method Not Allowed` |
| `it_coap_005` | Accept a valid remote command | Opt-in Confirmable PUT `2` | CoAP `2.04 Changed`; physical light output is **not** measured |

The actuation test is disabled unless explicitly requested. After a successful wake-up, both LED channels can **remain at full power** until turned off manually. CoAP response assertions verify acceptance, not optical behavior or timing. Queue-full behavior is not tested because it cannot be induced deterministically through this interface.
