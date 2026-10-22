# Thread dataset and provisioning

The device joins an existing Thread network. The operational dataset must match that network.

## Private build configuration

A private, untracked `.conf` overlay can supply Thread parameters. The example below contains **fabricated placeholder values**; do not join real networks with it and do not commit real credentials:

```ini
CONFIG_OPENTHREAD_PANID=0x1234
CONFIG_OPENTHREAD_CHANNEL=15
CONFIG_OPENTHREAD_NETWORK_NAME="EXAMPLE-NOT-A-REAL-NETWORK"
CONFIG_OPENTHREAD_XPANID="0011223344556677"
CONFIG_OPENTHREAD_NETWORKKEY="00112233445566778899aabbccddeeff"
CONFIG_WAKEUP_SRP_HOST_NAME="my-wakeup-h2"
CONFIG_WAKEUP_SRP_INSTANCE_NAME="My Wakeup Light H2"
```

To use your own private overlay in the build, pass it using Zephyr's `EXTRA_CONF_FILE` mechanism (the file path is resolved according to your Zephyr application/workspace invocation):

```sh
west build -p -b esp32h2_devkitm app/main_app -d build/esp32h2 -- -DEXTRA_CONF_FILE=/absolute/private/path/thread.conf
```

**A Network Key alone is not a complete Active Operational Dataset.** Other parameters include the mesh-local prefix, security policy and timestamps. For interoperability, provision the complete dataset from the existing network when supported. Do not publish operational keys or datasets.

Zephyr settings backed by NVS may preserve provisioned values across reflashes. Verify the active dataset on the device rather than assuming a changed build configuration overrides what is stored in flash. If an old dataset must be removed, deliberately clear the corresponding stored settings using an appropriate development procedure (and be aware that a full chip erase destroys other stored data).

The SRP host/instance name should be unique for each light. The Thread network needs a reachable SRP server to publish the service.
