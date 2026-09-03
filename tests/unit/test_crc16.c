/**
 * @file    test_crc16.c
 * @brief   Unit tests for the packet error control field.
 */
#include "minunity.h"
#include "pus/crc16.h"

void setUp(void)    { }
void tearDown(void) { }

/** @verifies SWREQ-CRC-010 */
static void test_crc_reference_vector(void)
{
    /* The check value published for CRC-16/CCITT-FALSE (poly 0x1021,
       init 0xFFFF, no reflection, no final XOR) over the ASCII string
       "123456789". Any change to the algorithm breaks this immediately. */
    const uint8_t vector[] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };

    TEST_ASSERT_EQUAL_HEX16(0x29B1u, crc16_ccitt(vector, sizeof(vector)));
}

/** @verifies SWREQ-CRC-010 */
static void test_crc_empty_buffer_is_seed(void)
{
    const uint8_t byte = 0x00u;

    TEST_ASSERT_EQUAL_HEX16(CRC16_SEED, crc16_ccitt(&byte, 0u));
}

/** @verifies SWREQ-CRC-010 */
static void test_crc_null_buffer_is_seed(void)
{
    TEST_ASSERT_EQUAL_HEX16(CRC16_SEED, crc16_ccitt(NULL, 10u));
}

/** @verifies SWREQ-CRC-010 */
static void test_crc_detects_single_bit_flip(void)
{
    uint8_t  data[8] = { 0x00u, 0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u };
    uint16_t before  = crc16_ccitt(data, sizeof(data));
    uint16_t after;

    data[3] ^= 0x01u;
    after = crc16_ccitt(data, sizeof(data));

    TEST_ASSERT_TRUE(before != after);
}

/** @verifies SWREQ-CRC-010 */
static void test_crc_is_position_sensitive(void)
{
    const uint8_t a[3] = { 0x01u, 0x02u, 0x03u };
    const uint8_t b[3] = { 0x03u, 0x02u, 0x01u };

    TEST_ASSERT_TRUE(crc16_ccitt(a, 3u) != crc16_ccitt(b, 3u));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_crc_reference_vector);
    RUN_TEST(test_crc_empty_buffer_is_seed);
    RUN_TEST(test_crc_null_buffer_is_seed);
    RUN_TEST(test_crc_detects_single_bit_flip);
    RUN_TEST(test_crc_is_position_sensitive);
    return UNITY_END();
}
