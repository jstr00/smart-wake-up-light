# Design decisions

## DR-01 — Separate portable logic from device integration

Pure C algorithms (light curve, quadrature decoder and CoAP payload parsing) live in `src/logic/`. `src/network/` contains Zephyr/OpenThread networking modules. Files directly in `src/` integrate hardware, threads and application startup. This makes the logic testable on a host while keeping hardware and protocol dependencies explicit.

## DR-02 — Use asynchronous light commands

The encoder and CoAP server enqueue commands to one light-control workqueue. A successful API call means queue admission, not completed PWM output. Queue overflow is reported as an error rather than blocking a networking callback.

## DR-03 — Enforce Router role and recover from Leader partitions

This installation has experienced undesired autonomous Thread partitions. After joining as Child, the device attempts Router promotion. On becoming Leader it disables Router eligibility and detaches, allowing reattachment to the existing network. This is a deployment-specific workaround, **not** a generally recommended OpenThread policy. The underlying API is [intended for testing and demonstrations](https://openthread.io/reference/group/api-thread-router). The `ot_manager_policy` interface allows other firmware to disable this behavior.

## DR-04 — Keep local extensions optional

The firmware builds without external local sources. Optional `local/Kconfig` and `local/app.cmake` hooks allow separately maintained modules without adding them to the public firmware configuration.

## DR-05 — Test at complementary levels

Twister/Ztest on `native_sim` exercises portable logic. Optional host-side pytest tests send real CoAP requests over the network. They do not verify physical light output; manual device tests remain separate.

## DR-06 — Use a shared 1 kHz PWM timer

Both LEDC PWM channels share a 1 kHz timer. This limits switching transitions compared with high-frequency PWM while keeping the channels synchronized. It is not a flicker-free guarantee; other frequencies may suit other hardware or viewing conditions. See [PWM and power calculations](pwm-and-power.md).