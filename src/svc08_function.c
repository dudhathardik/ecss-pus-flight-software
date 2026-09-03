/**
 * @file    svc08_function.c
 * @brief   PUS service 8 - function management.
 */
#include "pus/svc08_function.h"
#include "pus/pus_bytes.h"
#include "pus/hal.h"
#include "pus/obc_app.h"

/**
 * @implements SWREQ-SVC8-010
 * @implements SWREQ-SVC8-020
 * @implements SWREQ-SVC8-030
 * @implements SWREQ-SVC8-040
 * @implements SWREQ-SVC8-050
 * @implements SWREQ-SVC8-060
 * @implements SWREQ-SVC8-070
 * @implements SWREQ-ROB-020
 */
pus_status_t svc08_handle(const pus_tc_t *tc)
{
    uint8_t  fid;
    size_t   arg_len;
    uint8_t  arg8;
    pus_status_t st;

    if (tc == NULL) {
        return PUS_ERR_NULL;
    }
    if (tc->service_subtype != (uint8_t)PUS_ST08_PERFORM) {
        return PUS_ERR_UNKNOWN_SUBTYPE;
    }
    if (tc->app_data_len < 1u) {
        return PUS_ERR_BAD_LENGTH;
    }

    fid     = pus_get_u8(&tc->app_data[0]);
    arg_len = tc->app_data_len - 1u;

    switch (fid) {
    case OBC_FID_SET_MODE:
        if (arg_len != 1u) {
            st = PUS_ERR_BAD_LENGTH;
            break;
        }
        arg8 = pus_get_u8(&tc->app_data[1]);
        if ((arg8 != (uint8_t)OBC_MODE_SAFE) &&
            (arg8 != (uint8_t)OBC_MODE_NOMINAL)) {
            st = PUS_ERR_BAD_PARAMETER;
            break;
        }
        st = obc_set_mode((obc_mode_t)arg8);
        break;

    case OBC_FID_SET_HEATER:
        if (arg_len != 1u) {
            st = PUS_ERR_BAD_LENGTH;
            break;
        }
        arg8 = pus_get_u8(&tc->app_data[1]);
        if (arg8 > 1u) {
            st = PUS_ERR_BAD_PARAMETER;
            break;
        }
        /* The mode interlock lives in the application layer, which owns the
           heater line (SRS SWREQ-APP-080). */
        st = obc_set_heater((arg8 != 0u) ? PUS_TRUE : PUS_FALSE);
        break;

    case OBC_FID_INJECT_TEMP:
        if (arg_len != 2u) {
            st = PUS_ERR_BAD_LENGTH;
            break;
        }
        hal_inject_temperature(pus_get_i16(&tc->app_data[1]));
        st = PUS_OK;
        break;

    case OBC_FID_SET_TIME:
        if (arg_len != 6u) {
            st = PUS_ERR_BAD_LENGTH;
            break;
        }
        hal_set_cuc_time(pus_get_u32(&tc->app_data[1]),
                         pus_get_u16(&tc->app_data[5]));
        st = PUS_OK;
        break;

    case OBC_FID_RESET_COUNTERS:
        if (arg_len != 0u) {
            st = PUS_ERR_BAD_LENGTH;
            break;
        }
        obc_reset_counters();
        st = PUS_OK;
        break;

    default:
        st = PUS_ERR_UNKNOWN_FUNCTION;
        break;
    }

    return st;
}
