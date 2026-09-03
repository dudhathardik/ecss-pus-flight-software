/**
 * @file    test_ccsds.c
 * @brief   Unit tests for the CCSDS space packet primary header codec.
 */
#include "minunity.h"
#include "pus/ccsds.h"

void setUp(void)    { }
void tearDown(void) { }

static ccsds_primary_header_t reference_header(void)
{
    ccsds_primary_header_t h;

    h.version      = 0u;
    h.packet_type  = (uint8_t)CCSDS_TYPE_TC;
    h.sec_hdr_flag = 1u;
    h.apid         = 0x00Cu;
    h.seq_flags    = (uint8_t)CCSDS_SEQ_FLAG_UNSEGMENTED;
    h.seq_count    = 0x1234u;
    h.data_length  = 9u;
    return h;
}

/** @verifies SWREQ-CCSDS-010 */
static void test_pack_produces_the_expected_octets(void)
{
    ccsds_primary_header_t h = reference_header();
    uint8_t buf[CCSDS_PRIMARY_HEADER_LEN];

    TEST_ASSERT_EQUAL_INT(PUS_OK, ccsds_pack_primary(&h, buf, sizeof(buf)));

    /* 000 1 1 00000001100 -> 0x180C */
    TEST_ASSERT_EQUAL_HEX16(0x18u, buf[0]);
    TEST_ASSERT_EQUAL_HEX16(0x0Cu, buf[1]);
    /* 11 01001000110100 -> 0xD234 */
    TEST_ASSERT_EQUAL_HEX16(0xD2u, buf[2]);
    TEST_ASSERT_EQUAL_HEX16(0x34u, buf[3]);
    TEST_ASSERT_EQUAL_HEX16(0x00u, buf[4]);
    TEST_ASSERT_EQUAL_HEX16(0x09u, buf[5]);
}

/** @verifies SWREQ-CCSDS-010 */
static void test_pack_unpack_round_trip(void)
{
    ccsds_primary_header_t in = reference_header();
    ccsds_primary_header_t out;
    uint8_t buf[CCSDS_PRIMARY_HEADER_LEN];

    TEST_ASSERT_EQUAL_INT(PUS_OK, ccsds_pack_primary(&in, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_INT(PUS_OK, ccsds_unpack_primary(buf, sizeof(buf), &out));

    TEST_ASSERT_EQUAL_UINT8(in.version, out.version);
    TEST_ASSERT_EQUAL_UINT8(in.packet_type, out.packet_type);
    TEST_ASSERT_EQUAL_UINT8(in.sec_hdr_flag, out.sec_hdr_flag);
    TEST_ASSERT_EQUAL_UINT16(in.apid, out.apid);
    TEST_ASSERT_EQUAL_UINT8(in.seq_flags, out.seq_flags);
    TEST_ASSERT_EQUAL_UINT16(in.seq_count, out.seq_count);
    TEST_ASSERT_EQUAL_UINT16(in.data_length, out.data_length);
}

/**
 * @verifies SWREQ-CCSDS-010
 * Out-of-range field values must be masked, never allowed to corrupt the
 * neighbouring field.
 */
static void test_pack_masks_oversized_fields(void)
{
    ccsds_primary_header_t h = reference_header();
    ccsds_primary_header_t out;
    uint8_t buf[CCSDS_PRIMARY_HEADER_LEN];

    h.apid      = 0xFFFFu; /* 16 bits offered for an 11-bit field */
    h.seq_count = 0xFFFFu; /* 16 bits offered for a 14-bit field  */

    TEST_ASSERT_EQUAL_INT(PUS_OK, ccsds_pack_primary(&h, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_INT(PUS_OK, ccsds_unpack_primary(buf, sizeof(buf), &out));

    TEST_ASSERT_EQUAL_UINT16(0x07FFu, out.apid);
    TEST_ASSERT_EQUAL_UINT16(0x3FFFu, out.seq_count);
    TEST_ASSERT_EQUAL_UINT8(0u, out.version);
}

/** @verifies SWREQ-CCSDS-030 */
static void test_total_length_is_header_plus_data_plus_one(void)
{
    ccsds_primary_header_t h = reference_header();

    h.data_length = 9u;
    TEST_ASSERT_EQUAL_size_t(16u, ccsds_total_length(&h));

    h.data_length = 0u;
    TEST_ASSERT_EQUAL_size_t(7u, ccsds_total_length(&h));
}

/** @verifies SWREQ-ROB-010 */
static void test_rejects_null_and_short_buffers(void)
{
    ccsds_primary_header_t h = reference_header();
    ccsds_primary_header_t out;
    uint8_t buf[CCSDS_PRIMARY_HEADER_LEN];

    TEST_ASSERT_EQUAL_INT(PUS_ERR_NULL, ccsds_pack_primary(NULL, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_NULL, ccsds_pack_primary(&h, NULL, sizeof(buf)));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_BUFFER_TOO_SMALL,
                          ccsds_pack_primary(&h, buf, sizeof(buf) - 1u));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_NULL, ccsds_unpack_primary(NULL, 6u, &out));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_NULL, ccsds_unpack_primary(buf, 6u, NULL));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_TRUNCATED,
                          ccsds_unpack_primary(buf, 5u, &out));
    TEST_ASSERT_EQUAL_size_t(0u, ccsds_total_length(NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_pack_produces_the_expected_octets);
    RUN_TEST(test_pack_unpack_round_trip);
    RUN_TEST(test_pack_masks_oversized_fields);
    RUN_TEST(test_total_length_is_header_plus_data_plus_one);
    RUN_TEST(test_rejects_null_and_short_buffers);
    return UNITY_END();
}
