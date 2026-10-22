import ipaddress
import os
import threading
import time

SERVICE_TYPE = "_smart-wakeup._udp.local."


def build_uri(info, ip_version):
    path = info.properties.get(b"path")
    protocol = info.properties.get(b"proto")
    if protocol != b"coap" or path is None:
        raise ValueError("DNS-SD TXT must advertise proto=coap and path")
    path = path.decode("ascii")
    if not path.startswith("/") or "?" in path or "#" in path:
        raise ValueError("Invalid CoAP resource path in DNS-SD TXT")
    if not 1 <= info.port <= 65535:
        raise ValueError("Invalid SRV port in DNS-SD record")
    addresses = []
    for value in info.parsed_addresses(ip_version):
        address = ipaddress.IPv6Address(value)
        if not (address.is_link_local or address.is_loopback or address.is_unspecified or
                address.is_multicast):
            addresses.append(address)
    if not addresses:
        raise ValueError("No routable IPv6 address advertised for the service")
    addresses.sort(key=lambda address: not address.is_global)
    return f"coap://[{addresses[0].compressed}]:{info.port}{path}"


def discover_uri(timeout=8):
    from zeroconf import IPVersion, ServiceBrowser, ServiceStateChange, Zeroconf

    expected = os.environ.get("WAKEUP_INSTANCE")
    names = set()
    changed = threading.Event()
    name_lock = threading.Lock()

    def on_change(zeroconf, service_type, name, state_change):
        with name_lock:
            if state_change in (ServiceStateChange.Added, ServiceStateChange.Updated):
                names.add(name)
                changed.set()
            elif state_change is ServiceStateChange.Removed:
                names.discard(name)

    with Zeroconf(ip_version=IPVersion.All) as zeroconf:
        browser = ServiceBrowser(zeroconf, SERVICE_TYPE, handlers=[on_change])
        try:
            if not changed.wait(timeout):
                raise RuntimeError(
                    f"No {SERVICE_TYPE} advertisement found on the local network. "
                    "Check SRP registration and the Border Router mDNS advertising proxy."
                )
            time.sleep(0.5)
            suffix = "." + SERVICE_TYPE
            with name_lock:
                advertised = sorted(names)
            candidates = [name for name in advertised if name.endswith(suffix)]
            if expected:
                candidates = [name for name in candidates if name[:-len(suffix)] == expected]
            if not candidates:
                raise RuntimeError(
                    f"Requested service instance {expected!r} not found. "
                    f"Discovered: {advertised}"
                )
            if len(candidates) > 1:
                raise RuntimeError(
                    f"Multiple wake-up lights found: {candidates}. "
                    "Set WAKEUP_INSTANCE to choose one."
                )
            info = zeroconf.get_service_info(SERVICE_TYPE, candidates[0], timeout=3000)
            if info is None:
                raise RuntimeError(f"Failed to resolve DNS-SD SRV/TXT/AAAA for {candidates[0]}")
            return build_uri(info, IPVersion.V6Only)
        finally:
            browser.cancel()
