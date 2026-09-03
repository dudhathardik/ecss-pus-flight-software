/**
 * @file    svc17_test.c
 * @brief   PUS service 17 - connection test.
 */
#include "pus/svc17_test.h"
#include "pus/pus_tm.h"

/**
 * @implements SWREQ-SVC17-010
 * @implements SWREQ-SVC17-020
 */
pus_status_t svc17_handle(const pus_tc_t *tc)
{
    if (tc == NULL) {
        return PUS_ERR_NULL;
    }
    if (tc->service_subtype != (uint8_t)PUS_ST17_ARE_YOU_ALIVE) {
        return PUS_ERR_UNKNOWN_SUBTYPE;
    }
    if (tc->app_data_len != 0u) {
        return PUS_ERR_BAD_LENGTH;
    }

    return pus_tm_send((uint8_t)PUS_SERVICE_TEST,
                       (uint8_t)PUS_ST17_I_AM_ALIVE, NULL, 0u);
}
