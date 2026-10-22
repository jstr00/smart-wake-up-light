import asyncio
import os

import pytest


async def send_request(uri, code, payload):
    import aiocoap

    context = await aiocoap.Context.create_client_context()
    try:
        message = aiocoap.Message(
            transport_tuning=aiocoap.Reliable,
            code=code,
            uri=uri,
            payload=payload,
        )
        return (await asyncio.wait_for(context.request(message).response, 12)).code
    finally:
        await context.shutdown()


def request(uri, code, payload):
    return asyncio.run(send_request(uri, code, payload))


def test_it_discovery_001_srp_service_resolves(coap_uri):
    assert coap_uri.startswith("coap://[")
    assert coap_uri.endswith("/wakeup")


def test_it_coap_001_post_ping_returns_changed(coap_uri):
    aiocoap = pytest.importorskip("aiocoap")
    assert request(coap_uri, aiocoap.POST, b"ping") == aiocoap.CHANGED


def test_it_coap_002_put_ping_returns_changed(coap_uri):
    aiocoap = pytest.importorskip("aiocoap")
    assert request(coap_uri, aiocoap.PUT, b"ping") == aiocoap.CHANGED


@pytest.mark.parametrize(
    "payload",
    [
        pytest.param(b"", id="empty"),
        pytest.param(b"0", id="zero"),
        pytest.param(b"-1", id="negative"),
        pytest.param(b"+1", id="plus"),
        pytest.param(b"1.0", id="decimal"),
        pytest.param(b"a", id="non_numeric"),
        pytest.param(b" 2", id="leading_space"),
        pytest.param(b"2 ", id="trailing_space"),
        pytest.param(b"PING", id="wrong_case"),
        pytest.param(b"30\x000", id="embedded_nul"),
        pytest.param(b"999999999999999999999", id="overlong"),
    ],
)
def test_it_coap_003_invalid_payload_returns_bad_request(coap_uri, payload):
    aiocoap = pytest.importorskip("aiocoap")
    assert request(coap_uri, aiocoap.POST, payload) == aiocoap.BAD_REQUEST


def test_it_coap_004_get_is_not_allowed(coap_uri):
    aiocoap = pytest.importorskip("aiocoap")
    assert request(coap_uri, aiocoap.GET, b"") == aiocoap.METHOD_NOT_ALLOWED


@pytest.mark.actuating
@pytest.mark.skipif(
    os.environ.get("WAKEUP_ALLOW_ACTUATION") != "1",
    reason="Set WAKEUP_ALLOW_ACTUATION=1 to permit turning on the LEDs",
)
def test_it_coap_005_valid_wakeup_is_accepted(coap_uri):
    aiocoap = pytest.importorskip("aiocoap")
    assert request(coap_uri, aiocoap.PUT, b"2") == aiocoap.CHANGED
