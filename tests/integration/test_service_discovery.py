from types import SimpleNamespace

import pytest

from service_discovery import build_uri


class FakeIPv6Only:
    pass


def service(addresses, path=b"/wakeup", proto=b"coap", port=5683):
    return SimpleNamespace(
        properties={b"path": path, b"proto": proto},
        port=port,
        parsed_addresses=lambda version: addresses,
    )


def test_ut_discovery_001_uri_from_srv_txt_aaaa():
    info = service(["fd12::1234"])
    assert build_uri(info, FakeIPv6Only) == "coap://[fd12::1234]:5683/wakeup"


def test_ut_discovery_002_reject_link_local_only():
    with pytest.raises(ValueError, match="routable IPv6"):
        build_uri(service(["fe80::1"]), FakeIPv6Only)


@pytest.mark.parametrize(
    "info",
    [
        service(["fd12::1"], proto=b"http"),
        service(["fd12::1"], path=b"wakeup"),
        service(["fd12::1"], port=0),
        service([]),
    ],
    ids=["wrong_protocol", "invalid_path", "invalid_port", "no_address"],
)
def test_ut_discovery_003_reject_invalid_service_records(info):
    with pytest.raises(ValueError):
        build_uri(info, FakeIPv6Only)
