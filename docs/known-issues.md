# Known limitations and roadmap

## Limitations

- CoAP control is unauthenticated and unencrypted. Any host that can reach UDP port 5683 on the device can send commands. Use a trusted network and appropriate firewall rules.
- Forced Router promotion and Leader detachment are enabled for this installation. This non-standard OpenThread policy can interrupt routing and cannot resolve every partition problem. See [decision records](decision-records.md).
- CoAP `2.04 Changed` acknowledges that a command was queued, not that the physical PWM output succeeded. The application does not expose completion status.
- Encoder rotation is accumulated in the input worker. Opposite turns may cancel before processing, and intermediate button/turn ordering is not guaranteed.
- The SRP module registers one service per firmware instance.

## Roadmap

- CoAPS with authenticated and authorized clients.
- A more convenient Thread network provisioning workflow.
