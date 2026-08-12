# NOTE: written against the real twister_harness DeviceAdapter API
# (readlines_until) and this project's actual log strings, but not
# verified against real hardware - no nRF5340 DK was available in the
# environment this was written in. Verify against real hardware before
# trusting it; see docs/testing_ecosystem.md.

import logging

from twister_harness import DeviceAdapter

logger = logging.getLogger(__name__)


def test_bluetooth_initializes(dut: DeviceAdapter):
    logger.info('waiting for Bluetooth to initialize')
    dut.readlines_until(regex='Bluetooth initialized', timeout=10)


def test_advertising_starts(dut: DeviceAdapter):
    logger.info('waiting for advertising to start')
    dut.readlines_until(regex='Advertising started', timeout=10)
