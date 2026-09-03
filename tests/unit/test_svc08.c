/**
 * @file    test_svc08.c
 * @brief   Unit tests for function management.
 */
#include "minunity.h"
#include "test_support.h"

#include "pus/hal.h"
#include "pus/obc_app.h"
#include "pus/pus_bytes.h"
#include "pus/svc08_function.h"
#include "pus/pus_tm.h"

void setUp(void)
{
    obc_init();
    ts_tm_flush();
}

void tearDown(void) { }

static pus_status_t call_function(uint8_t fid, const uint8_t *args, size_t n)
{
    uint8_t app[8];
    size_t  i;

    app[0] = fid;
    for (i = 0u; i < n; ++i) {
        app[1u + i] = args[i];
    }
    return ts_send_tc(8u, (uint8_t)PUS_ST08_PERFORM, app, 1u + n);
}

/**
 * @verifies SWREQ-SVC8-010
 * @verifies SWREQ-SVC8-020
 */
static void test_set_mode_switches_the_on_board_mode(void)
{
    const uint8_t nominal[1] = { (uint8_t)OBC_MODE_NOMINAL };
    const uint8_t safe[1]    = { (uint8_t)OBC_MODE_SAFE };

    TEST_ASSERT_EQUAL_INT(OBC_MODE_SAFE, obc_get_state()->mode);
    TEST_ASSERT_EQUAL_INT(PUS_OK, call_function(OBC_FID_SET_MODE, nominal, 1u));
    TEST_ASSERT_EQUAL_INT(OBC_MODE_NOMINAL, obc_get_state()->mode);
    TEST_ASSERT_EQUAL_INT(PUS_OK, call_function(OBC_FID_SET_MODE, safe, 1u));
    TEST_ASSERT_EQUAL_INT(OBC_MODE_SAFE, obc_get_state()->mode);
}

/** @verifies SWREQ-SVC8-020 */
static void test_set_mode_rejects_an_undefined_mode(void)
{
    const uint8_t bad[1] = { 2u };

    TEST_ASSERT_EQUAL_INT(PUS_ERR_BAD_PARAMETER,
                          call_function(OBC_FID_SET_MODE, bad, 1u));
    TEST_ASSERT_EQUAL_INT(OBC_MODE_SAFE, obc_get_state()->mode);
}

/** @verifies SWREQ-SVC8-030 */
static void test_manual_heater_command_is_inhibited_in_safe_mode(void)
{
    const uint8_t on[1] = { 1u };

    TEST_ASSERT_EQUAL_INT(OBC_MODE_SAFE, obc_get_state()->mode);
    TEST_ASSERT_EQUAL_INT(PUS_ERR_NOT_PERMITTED,
                          call_function(OBC_FID_SET_HEATER, on, 1u));
    TEST_ASSERT_FALSE(hal_get_heater());
}

/** @verifies SWREQ-SVC8-030 */
static void test_heater_can_be_commanded_in_nominal_mode(void)
{
    const uint8_t nominal[1] = { (uint8_t)OBC_MODE_NOMINAL };
    const uint8_t on[1]      = { 1u };
    const uint8_t off[1]     = { 0u };
    const uint8_t bad[1]     = { 2u };

    (void)call_function(OBC_FID_SET_MODE, nominal, 1u);

    TEST_ASSERT_EQUAL_INT(PUS_OK, call_function(OBC_FID_SET_HEATER, on, 1u));
    TEST_ASSERT_TRUE(hal_get_heater());
    TEST_ASSERT_EQUAL_INT(PUS_OK, call_function(OBC_FID_SET_HEATER, off, 1u));
    TEST_ASSERT_FALSE(hal_get_heater());
    TEST_ASSERT_EQUAL_INT(PUS_ERR_BAD_PARAMETER,
                          call_function(OBC_FID_SET_HEATER, bad, 1u));
}

/** @verifies SWREQ-SVC8-040 */
static void test_temperature_injection_reaches_the_application(void)
{
    uint8_t arg[2];

    pus_put_i16(arg, -250);
    TEST_ASSERT_EQUAL_INT(PUS_OK, call_function(OBC_FID_INJECT_TEMP, arg, 2u));
    TEST_ASSERT_EQUAL_INT(-250, hal_read_temperature());

    obc_tick();
    TEST_ASSERT_EQUAL_INT(-250, obc_get_state()->temperature_dc);
}

/** @verifies SWREQ-APP-090 */
static void test_counters_can_be_reset_from_ground(void)
{
    (void)ts_send_tc(17u, 1u, NULL, 0u);
    TEST_ASSERT_TRUE(obc_get_state()->tc_accepted > 0u);

    TEST_ASSERT_EQUAL_INT(PUS_OK, call_function(OBC_FID_RESET_COUNTERS, NULL, 0u));
    /* The reset telecommand is counted on acceptance and then cleared by
       its own execution, so the counter ends at zero. */
    TEST_ASSERT_EQUAL_UINT16(0u, obc_get_state()->tc_accepted);
    TEST_ASSERT_EQUAL_UINT16(0u, obc_get_state()->tc_rejected);
}

/**
 * @verifies SWREQ-SVC8-070
 * @verifies SWREQ-HAL-020
 */
static void test_on_board_time_can_be_set_from_ground(void)
{
    uint8_t  args[6];
    uint32_t coarse = 0u;
    uint16_t fine   = 0u;

    pus_put_u32(&args[0], 1000000u);
    pus_put_u16(&args[4], 0x8000u);

    TEST_ASSERT_EQUAL_INT(PUS_OK, call_function(OBC_FID_SET_TIME, args, 6u));

    hal_get_cuc_time(&coarse, &fine);
    TEST_ASSERT_EQUAL_UINT32(1000000u, coarse);
    TEST_ASSERT_EQUAL_UINT16(0x8000u, fine);

    /* The commanded value is an epoch, not a freeze: time keeps running. */
    obc_tick();
    hal_get_cuc_time(&coarse, &fine);
    TEST_ASSERT_EQUAL_UINT32(1000000u, coarse);
    TEST_ASSERT_TRUE(fine > 0x8000u);
}

/** @verifies SWREQ-SVC8-050 */
static void test_unknown_function_identifier_is_rejected(void)
{
    TEST_ASSERT_EQUAL_INT(PUS_ERR_UNKNOWN_FUNCTION,
                          call_function(0u, NULL, 0u));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_UNKNOWN_FUNCTION,
                          call_function(0xFFu, NULL, 0u));
}

/** @verifies SWREQ-SVC8-060 */
static void test_wrong_argument_lengths_are_rejected(void)
{
    const uint8_t two[2] = { 0u, 0u };
    const uint8_t one[1] = { 0u };

    TEST_ASSERT_EQUAL_INT(PUS_ERR_BAD_LENGTH,
                          call_function(OBC_FID_SET_MODE, NULL, 0u));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_BAD_LENGTH,
                          call_function(OBC_FID_SET_MODE, two, 2u));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_BAD_LENGTH,
                          call_function(OBC_FID_SET_HEATER, two, 2u));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_BAD_LENGTH,
                          call_function(OBC_FID_INJECT_TEMP, one, 1u));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_BAD_LENGTH,
                          call_function(OBC_FID_RESET_COUNTERS, one, 1u));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_BAD_LENGTH,
                          call_function(OBC_FID_SET_TIME, two, 2u));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_BAD_LENGTH,
                          ts_send_tc(8u, (uint8_t)PUS_ST08_PERFORM, NULL, 0u));
}

/** @verifies SWREQ-ROB-020 */
static void test_unknown_subtype_and_null_are_rejected(void)
{
    TEST_ASSERT_EQUAL_INT(PUS_ERR_UNKNOWN_SUBTYPE, ts_send_tc(8u, 9u, NULL, 0u));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_NULL, svc08_handle(NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_set_mode_switches_the_on_board_mode);
    RUN_TEST(test_set_mode_rejects_an_undefined_mode);
    RUN_TEST(test_manual_heater_command_is_inhibited_in_safe_mode);
    RUN_TEST(test_heater_can_be_commanded_in_nominal_mode);
    RUN_TEST(test_temperature_injection_reaches_the_application);
    RUN_TEST(test_counters_can_be_reset_from_ground);
    RUN_TEST(test_on_board_time_can_be_set_from_ground);
    RUN_TEST(test_unknown_function_identifier_is_rejected);
    RUN_TEST(test_wrong_argument_lengths_are_rejected);
    RUN_TEST(test_unknown_subtype_and_null_are_rejected);
    return UNITY_END();
}
