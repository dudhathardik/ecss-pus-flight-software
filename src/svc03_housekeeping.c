/**
 * @file    svc03_housekeeping.c
 * @brief   PUS service 3 - housekeeping reporting.
 */
#include "pus/svc03_housekeeping.h"
#include "pus/pus_bytes.h"
#include "pus/pus_config.h"
#include "pus/pus_tm.h"
#include "pus/tm_queue.h"
#include "pus/obc_app.h"

/** Source data length of the essential report (SID 1), CRC excluded. */
#define SVC03_SID1_DATA_LEN   9u
/** Source data length of the diagnostic report (SID 2), CRC excluded. */
#define SVC03_SID2_DATA_LEN   10u

/** Periodic generation flag, indexed by (sid - 1). */
static pus_bool_t s_enabled[OBC_HK_MAX_STRUCTURES];
/** Minor cycles elapsed inside the current housekeeping period. */
static uint32_t   s_phase;

/** @implements SWREQ-SVC3-010 */
static pus_bool_t svc03_sid_valid(uint8_t sid)
{
    return ((sid >= 1u) && (sid <= (uint8_t)OBC_HK_MAX_STRUCTURES))
         ? PUS_TRUE : PUS_FALSE;
}

/** @implements SWREQ-SVC3-070 */
void svc03_init(void)
{
    size_t i;

    for (i = 0u; i < PUS_ARRAY_LEN(s_enabled); ++i) {
        s_enabled[i] = PUS_FALSE;
    }
    /* The essential report is generated from power-on so that the ground
       segment sees the unit as soon as the downlink is established. */
    s_enabled[OBC_HK_SID_ESSENTIAL - 1u] = PUS_TRUE;
    s_phase = 0u;
}

pus_bool_t svc03_is_enabled(uint8_t sid)
{
    if (svc03_sid_valid(sid) == PUS_FALSE) {
        return PUS_FALSE;
    }
    return s_enabled[sid - 1u];
}

/**
 * @implements SWREQ-SVC3-020
 * @implements SWREQ-SVC3-060
 *
 * The layouts below are frozen by ICD.md; changing a field order or width is
 * a ground database change and must go through the ICD, not through this file.
 */
pus_status_t svc03_generate(uint8_t sid)
{
    const obc_state_t *st = obc_get_state();
    uint8_t data[SVC03_SID2_DATA_LEN];
    size_t  len;

    if (svc03_sid_valid(sid) == PUS_FALSE) {
        return PUS_ERR_UNKNOWN_SID;
    }

    if (sid == (uint8_t)OBC_HK_SID_ESSENTIAL) {
        pus_put_u8(&data[0], sid);
        pus_put_u8(&data[1], (uint8_t)st->mode);
        pus_put_u32(&data[2], st->uptime_ticks);
        pus_put_i16(&data[6], st->temperature_dc);
        pus_put_u8(&data[8], st->heater_on);
        len = (size_t)SVC03_SID1_DATA_LEN;
    } else {
        pus_put_u8(&data[0], sid);
        pus_put_u16(&data[1], st->tc_accepted);
        pus_put_u16(&data[3], st->tc_rejected);
        pus_put_u16(&data[5], st->last_failure_code);
        pus_put_u8(&data[7], (uint8_t)tm_queue_count());
        pus_put_u16(&data[8], tm_queue_overflow_count());
        len = (size_t)SVC03_SID2_DATA_LEN;
    }

    return pus_tm_send((uint8_t)PUS_SERVICE_HOUSEKEEPING,
                       (uint8_t)PUS_ST03_REPORT, data, len);
}

/**
 * @implements SWREQ-SVC3-040
 * @implements SWREQ-SVC3-050
 * @implements SWREQ-ROB-020
 *
 * As for service 5, the whole structure identifier list is validated before
 * the first flag is written so that a rejected telecommand has no effect.
 */
static pus_status_t svc03_apply_list(const pus_tc_t *tc, pus_bool_t enable,
                                     pus_bool_t one_shot)
{
    uint8_t count;
    uint8_t i;
    uint8_t sid;
    pus_status_t st = PUS_OK;

    if (tc->app_data_len < 1u) {
        return PUS_ERR_BAD_LENGTH;
    }

    count = pus_get_u8(&tc->app_data[0]);
    if (tc->app_data_len != (size_t)(1u + (size_t)count)) {
        return PUS_ERR_BAD_LENGTH;
    }
    if (count == 0u) {
        return PUS_ERR_BAD_PARAMETER;
    }

    for (i = 0u; i < count; ++i) {
        if (svc03_sid_valid(pus_get_u8(&tc->app_data[1u + i])) == PUS_FALSE) {
            return PUS_ERR_UNKNOWN_SID;
        }
    }

    for (i = 0u; i < count; ++i) {
        sid = pus_get_u8(&tc->app_data[1u + i]);
        if (one_shot != PUS_FALSE) {
            st = svc03_generate(sid);
            if (st != PUS_OK) {
                break;
            }
        } else {
            s_enabled[sid - 1u] = enable;
        }
    }

    return st;
}

pus_status_t svc03_handle(const pus_tc_t *tc)
{
    pus_status_t st;

    if (tc == NULL) {
        return PUS_ERR_NULL;
    }

    switch (tc->service_subtype) {
    case PUS_ST03_ENABLE:
        st = svc03_apply_list(tc, PUS_TRUE, PUS_FALSE);
        break;
    case PUS_ST03_DISABLE:
        st = svc03_apply_list(tc, PUS_FALSE, PUS_FALSE);
        break;
    case PUS_ST03_ONE_SHOT:
        st = svc03_apply_list(tc, PUS_FALSE, PUS_TRUE);
        break;
    default:
        st = PUS_ERR_UNKNOWN_SUBTYPE;
        break;
    }

    return st;
}

/** @implements SWREQ-SVC3-030 */
void svc03_tick(void)
{
    uint8_t sid;

    s_phase += 1u;
    if (s_phase < (uint32_t)OBC_HK_PERIOD_TICKS) {
        return;
    }
    s_phase = 0u;

    for (sid = 1u; sid <= (uint8_t)OBC_HK_MAX_STRUCTURES; ++sid) {
        if (s_enabled[sid - 1u] != PUS_FALSE) {
            (void)svc03_generate(sid);
        }
    }
}
