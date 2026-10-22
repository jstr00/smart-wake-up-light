# Tests

The coverage and expected results of individual tests are listed in [TEST_MATRIX.md](TEST_MATRIX.md). Test names carry stable `ut_*` and `it_*` identifiers.

## Unit tests (Twister)

From the repository root inside an initialized Zephyr 4.3 workspace:

```sh
west twister -T tests/twister -p native_sim --inline-logs
```

Twister builds the portable C logic using Ztest on the host. It does not require a connected ESP32-H2. The test discovery file is named `testcase.yaml` for Zephyr 4.3 compatibility.

## Python tests and live CoAP integration tests

Install Python dependencies (ideally in a virtual environment):

```sh
python -m pip install -r tests/integration/requirements.txt
```

Run only the network-independent discovery helper tests:

```sh
python -m pytest tests/integration/test_service_discovery.py -v
```

Run the entire pytest suite from the host PC on the same infrastructure LAN as the Thread Border Router:

```sh
python -m pytest tests/integration -v -m 'not actuating'
```

The live tests discover `_smart-wakeup._udp.local.` via the Border Router's **mDNS/DNS-SD advertising proxy**. They read the SRV port, TXT `path` and an IPv6 AAAA address; **no manual IP address is needed**. When multiple devices advertise this service type, select the intended instance by name:

```sh
WAKEUP_INSTANCE='Smart Wakeup Light H2' python -m pytest tests/integration -v -m 'not actuating'
```

The host must have IPv6 connectivity to the Thread device's advertised address. The SRP registration alone is not sufficient: the Border Router must advertise it to the LAN. For discovery diagnostics on Linux, run `avahi-browse -rt _smart-wakeup._udp` (package `avahi-utils`). If discovery fails, pytest reports a failure instead of silently skipping live checks.

The valid wake-up test changes the LED output. **Both channels may stay at full brightness after the test.** Run it only when the light is safe to activate, then turn it off manually:

```sh
WAKEUP_ALLOW_ACTUATION=1 python -m pytest tests/integration -v -m actuating
```

These network tests do not flash devices, test LED brightness, prove animation timing, or deliberately overflow the command queue.
