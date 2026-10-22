import pytest

from service_discovery import discover_uri


@pytest.fixture(scope="session")
def coap_uri():
    pytest.importorskip("zeroconf")
    try:
        uri = discover_uri()
    except (OSError, RuntimeError, ValueError) as error:
        pytest.fail(f"DNS-SD service discovery failed: {error}")
    return uri
