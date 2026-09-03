/**
 * @file    test_pus_tc.c
 * @brief   Unit tests for the telecommand acceptance checks.
 *
 * One test case per rejection path: an acceptance check that silently stops
 * working is the classic way for a ground segment to lose the ability to
 * distinguish a corrupted uplink from a mis-addressed one.
 */
#include "minunity.h"
#include "test_support.h"

#include "pus/pus_bytes.h"
#include "pus/svc17_test.h"

static uint8_t s_tc[PUS_MAX_PACKET_SIZE];
static size_t  s_len;
static pus_tc_t s_parsed;

void setUp(void)
{
    s_len = ts_build_tc(s_tc, sizeof(s_tc),
                        (uint8_t)PUS_SERVICE_TEST,
                        (uint8_t)PUS_ST17_ARE_YOU_ALIVE,
                        (uint8_t)PUS_ACK_ACCEPTANCE | (uint8_t)PUS_ACK_COMPLETION,
                        NULL, 0u);
}

void tearDown(void) { }

/** @verifies SWREQ-TC-010 */
static void test_parses_a_well_formed_telecommand(void)
{
    TEST_ASSERT_EQUAL_size_t(13u, s_len);
    TEST_ASSERT_EQUAL_INT(PUS_OK, pus_tc_parse(s_tc, s_len, &s_parsed));
    TEST_ASSERT_EQUAL_UINT8(PUS_SERVICE_TEST, s_parsed.service_type);
    TEST_ASSERT_EQUAL_UINT8(PUS_ST17_ARE_YOU_ALIVE, s_parsed.service_subtype);
    TEST_ASSERT_EQUAL_UINT8(PUS_VERSION_C, s_parsed.pus_version);
    TEST_ASSERT_EQUAL_UINT16(PUS_TC_APID, s_parsed.prim.apid);
    TEST_ASSERT_EQUAL_size_t(0u, s_parsed.app_data_len);
    TEST_ASSERT_NULL(s_parsed.app_data);
}

/** @verifies SWREQ-TC-010 */
static void test_exposes_application_data(void)
{
    const uint8_t payload[3] = { 0xAAu, 0xBBu, 0xCCu };

    s_len = ts_build_tc(s_tc, sizeof(s_tc), 8u, 1u, 0u, payload, sizeof(payload));

    TEST_ASSERT_EQUAL_INT(PUS_OK, pus_tc_parse(s_tc, s_len, &s_parsed));
    TEST_ASSERT_EQUAL_size_t(3u, s_parsed.app_data_len);
    TEST_ASSERT_NOT_NULL(s_parsed.app_data);
    TEST_ASSERT_EQUAL_MEMORY(payload, s_parsed.app_data, 3u);
}

/** @verifies SWREQ-SVC1-040 */
static void test_request_id_is_the_first_four_header_octets(void)
{
    uint32_t expected = pus_get_u32(s_tc);

    TEST_ASSERT_EQUAL_INT(PUS_OK, pus_tc_parse(s_tc, s_len, &s_parsed));
    TEST_ASSERT_EQUAL_UINT32(expected, s_parsed.request_id);
}

/** @verifies SWREQ-TC-080 */
static void test_acknowledgement_flags_are_decoded(void)
{
    TEST_ASSERT_EQUAL_INT(PUS_OK, pus_tc_parse(s_tc, s_len, &s_parsed));
    TEST_ASSERT_TRUE(pus_tc_ack_requested(&s_parsed, PUS_ACK_ACCEPTANCE));
    TEST_ASSERT_TRUE(pus_tc_ack_requested(&s_parsed, PUS_ACK_COMPLETION));
    TEST_ASSERT_FALSE(pus_tc_ack_requested(&s_parsed, PUS_ACK_START));
    TEST_ASSERT_FALSE(pus_tc_ack_requested(NULL, PUS_ACK_ACCEPTANCE));
}

/** @verifies SWREQ-TC-020 */
static void test_rejects_a_packet_shorter_than_the_minimum(void)
{
    TEST_ASSERT_EQUAL_INT(PUS_ERR_TRUNCATED, pus_tc_parse(s_tc, 12u, &s_parsed));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_TRUNCATED, pus_tc_parse(s_tc, 0u, &s_parsed));
}

/** @verifies SWREQ-TC-030 */
static void test_rejects_an_inconsistent_declared_length(void)
{
    pus_put_u16(&s_tc[4], (uint16_t)(s_len - 6u)); /* one octet too many */
    ts_fix_crc(s_tc, s_len);

    TEST_ASSERT_EQUAL_INT(PUS_ERR_BAD_LENGTH, pus_tc_parse(s_tc, s_len, &s_parsed));
}

/** @verifies SWREQ-CRC-020 */
static void test_rejects_a_corrupted_packet(void)
{
    ts_break_crc(s_tc, s_len);
    TEST_ASSERT_EQUAL_INT(PUS_ERR_BAD_CRC, pus_tc_parse(s_tc, s_len, &s_parsed));
}

/**
 * @verifies SWREQ-TC-015
 * A packet that is both corrupted and mis-addressed must be reported as a
 * CRC failure: the APID cannot be trusted until the CRC verifies.
 */
static void test_crc_is_checked_before_the_apid(void)
{
    s_tc[1] = 0x7Fu;          /* wrong APID */
    ts_break_crc(s_tc, s_len); /* and a corrupted checksum */

    TEST_ASSERT_EQUAL_INT(PUS_ERR_BAD_CRC, pus_tc_parse(s_tc, s_len, &s_parsed));
}

/** @verifies SWREQ-CCSDS-020 */
static void test_rejects_a_bad_ccsds_version(void)
{
    s_tc[0] = (uint8_t)(s_tc[0] | 0xE0u);
    ts_fix_crc(s_tc, s_len);
    TEST_ASSERT_EQUAL_INT(PUS_ERR_BAD_VERSION, pus_tc_parse(s_tc, s_len, &s_parsed));
}

/** @verifies SWREQ-TC-040 */
static void test_rejects_a_telemetry_packet_on_the_uplink(void)
{
    s_tc[0] = (uint8_t)(s_tc[0] & 0xEFu); /* clear the packet type bit */
    ts_fix_crc(s_tc, s_len);
    TEST_ASSERT_EQUAL_INT(PUS_ERR_BAD_PACKET_TYPE,
                          pus_tc_parse(s_tc, s_len, &s_parsed));
}

/** @verifies SWREQ-TC-050 */
static void test_rejects_a_packet_without_a_secondary_header(void)
{
    s_tc[0] = (uint8_t)(s_tc[0] & 0xF7u);
    ts_fix_crc(s_tc, s_len);
    TEST_ASSERT_EQUAL_INT(PUS_ERR_NO_SEC_HEADER,
                          pus_tc_parse(s_tc, s_len, &s_parsed));
}

/** @verifies SWREQ-TC-060 */
static void test_rejects_a_foreign_apid(void)
{
    s_tc[1] = (uint8_t)(PUS_TC_APID + 1u);
    ts_fix_crc(s_tc, s_len);
    TEST_ASSERT_EQUAL_INT(PUS_ERR_BAD_APID, pus_tc_parse(s_tc, s_len, &s_parsed));
}

/** @verifies SWREQ-TC-070 */
static void test_rejects_a_pus_a_telecommand(void)
{
    s_tc[6] = (uint8_t)((1u << 4) | (s_tc[6] & 0x0Fu)); /* PUS version 1 */
    ts_fix_crc(s_tc, s_len);
    TEST_ASSERT_EQUAL_INT(PUS_ERR_BAD_VERSION, pus_tc_parse(s_tc, s_len, &s_parsed));
}

/** @verifies SWREQ-ROB-010 */
static void test_rejects_null_arguments(void)
{
    TEST_ASSERT_EQUAL_INT(PUS_ERR_NULL, pus_tc_parse(NULL, s_len, &s_parsed));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_NULL, pus_tc_parse(s_tc, s_len, NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_parses_a_well_formed_telecommand);
    RUN_TEST(test_exposes_application_data);
    RUN_TEST(test_request_id_is_the_first_four_header_octets);
    RUN_TEST(test_acknowledgement_flags_are_decoded);
    RUN_TEST(test_rejects_a_packet_shorter_than_the_minimum);
    RUN_TEST(test_rejects_an_inconsistent_declared_length);
    RUN_TEST(test_rejects_a_corrupted_packet);
    RUN_TEST(test_crc_is_checked_before_the_apid);
    RUN_TEST(test_rejects_a_bad_ccsds_version);
    RUN_TEST(test_rejects_a_telemetry_packet_on_the_uplink);
    RUN_TEST(test_rejects_a_packet_without_a_secondary_header);
    RUN_TEST(test_rejects_a_foreign_apid);
    RUN_TEST(test_rejects_a_pus_a_telecommand);
    RUN_TEST(test_rejects_null_arguments);
    return UNITY_END();
}
