"""Host-side simulations of recovered on-air struct layouts.

These tests do not exercise ESP-NOW or hardware. They make the ABI assumptions
and cross-version incompatibility explicit and repeatable.
"""

import struct
import unittest


MAC = bytes([1, 2, 3, 4, 5, 6])


def encode_protocol_a(
    msg_type: int,
    mac: bytes,
    cable: int,
    status: int,
    data1: int,
    data2: int,
    length: int,
    timestamp: int,
) -> bytes:
    # uint8 enum, six-byte MAC, five MIDI bytes, aligned uint32 timestamp.
    return struct.pack("<B6sBBBBBI", msg_type, mac, cable, status, data1, data2, length, timestamp)


def encode_protocol_b(
    msg_type: int,
    mac: bytes,
    cable: int,
    status: int,
    data1: int,
    data2: int,
    length: int,
) -> bytes:
    # Default 32-bit enum and one trailing ABI padding byte.
    return struct.pack("<i6sBBBBBx", msg_type, mac, cable, status, data1, data2, length)


def recovered_device2_cin_length(cin: int, status: int) -> int | None:
    if cin in {0x8, 0x9, 0xA, 0xB, 0xE}:
        return 3
    if cin in {0xC, 0xD}:
        return 2
    if cin == 0xF:
        if status in {0xF6, 0xF8, 0xFA, 0xFB, 0xFC, 0xFE, 0xFF}:
            return 1
        if status in {0xF1, 0xF3}:
            return 2
        return 3
    return None


class ProtocolLayoutTests(unittest.TestCase):
    def test_both_recovered_abis_are_16_bytes(self) -> None:
        a = encode_protocol_a(5, MAC, 2, 0x90, 60, 100, 3, 0x12345678)
        b = encode_protocol_b(5, MAC, 2, 0x90, 60, 100, 3)
        self.assertEqual(len(a), 16)
        self.assertEqual(len(b), 16)

    def test_equal_size_does_not_mean_compatible(self) -> None:
        a = encode_protocol_a(5, MAC, 2, 0x90, 60, 100, 3, 0x12345678)
        b = encode_protocol_b(5, MAC, 2, 0x90, 60, 100, 3)
        self.assertNotEqual(a, b)
        # Protocol B interpreted as A sees the MIDI status as its length.
        self.assertEqual(b[11], 0x90)
        # Protocol A interpreted as B sees type plus three MAC bytes as enum.
        self.assertEqual(struct.unpack_from("<I", a)[0], 0x03020105)

    def test_protocol_a_field_offsets(self) -> None:
        payload = encode_protocol_a(5, MAC, 2, 0x90, 60, 100, 3, 0x12345678)
        self.assertEqual(payload[0], 5)
        self.assertEqual(payload[1:7], MAC)
        self.assertEqual(payload[7:12], bytes([2, 0x90, 60, 100, 3]))
        self.assertEqual(payload[12:16], bytes([0x78, 0x56, 0x34, 0x12]))

    def test_protocol_b_field_offsets(self) -> None:
        payload = encode_protocol_b(5, MAC, 2, 0x90, 60, 100, 3)
        self.assertEqual(payload[0:4], bytes([5, 0, 0, 0]))
        self.assertEqual(payload[4:10], MAC)
        self.assertEqual(payload[10:15], bytes([2, 0x90, 60, 100, 3]))

    def test_device2_decoder_omits_sysex_cins(self) -> None:
        for cin in (0x4, 0x5, 0x6, 0x7):
            self.assertIsNone(recovered_device2_cin_length(cin, 0xF0))

    def test_device2_decoder_handles_channel_voice_lengths(self) -> None:
        self.assertEqual(recovered_device2_cin_length(0x9, 0x90), 3)
        self.assertEqual(recovered_device2_cin_length(0xC, 0xC0), 2)
        self.assertEqual(recovered_device2_cin_length(0xF, 0xF8), 1)


if __name__ == "__main__":
    unittest.main()
