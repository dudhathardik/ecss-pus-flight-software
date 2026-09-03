/**
 * @file    svc05_event.c
 * @brief   PUS service 5 - event reporting.
 */
#include "pus/svc05_event.h"
#include "pus/pus_bytes.h"
#include "pus/pus_config.h"
#include "pus/pus_tm.h"

/** Event report source data: event identifier plus one auxiliary word. */
#define SVC05_REPORT_DATA_LEN  4u

/** One flag per event identifier, indexed by (id - 1). */
static pus_bool_t s_enabled[OBC_EVENT_MAX_IDS];

static pus_bool_t svc05_id_valid(uint16_t id)
{
    return ((id >= 1u) && (id <= (uint16_t)OBC_EVENT_MAX_IDS))
         ? PUS_TRUE : PUS_FALSE;
}

void svc05_init(void)
{
    size_t i;

    for (i = 0u; i < PUS_ARRAY_LEN(s_enabled); ++i) {
        s_enabled[i] = PUS_TRUE;
    }
}

/**
 * @implements SWREQ-SVC5-010
 * @implements SWREQ-SVC5-030
 */
pus_status_t svc05_report(obc_event_id_t id, uint8_t severity, uint16_t aux)
{
    uint8_t data[SVC05_REPORT_DATA_LEN];

    if (svc05_id_valid((uint16_t)id) == PUS_FALSE) {
        return PUS_ERR_UNKNOWN_EVENT;
    }
    if ((severity < (uint8_t)PUS_ST05_INFORMATIVE) ||
        (severity > (uint8_t)PUS_ST05_HIGH_SEVERITY)) {
        return PUS_ERR_BAD_PARAMETER;
    }
    if (s_enabled[(uint16_t)id - 1u] == PUS_FALSE) {
        return PUS_OK; /* silently suppressed, as commanded by ground */
    }

    pus_put_u16(&data[0], (uint16_t)id);
    pus_put_u16(&data[2], aux);

    return pus_tm_send((uint8_t)PUS_SERVICE_EVENT, severity,
                       data, sizeof(data));
}

pus_bool_t svc05_is_enabled(obc_event_id_t id)
{
    if (svc05_id_valid((uint16_t)id) == PUS_FALSE) {
        return PUS_FALSE;
    }
    return s_enabled[(uint16_t)id - 1u];
}

/**
 * @implements SWREQ-SVC5-020
 * @implements SWREQ-SVC5-040
 * @implements SWREQ-ROB-020
 *
 * The list is validated in full before any flag is written, so a telecommand
 * containing one bad identifier leaves the on-board configuration untouched
 * instead of applying a partial update the ground segment cannot predict.
 */
static pus_status_t svc05_set_enabled(const pus_tc_t *tc, pus_bool_t value)
{
    uint8_t  count;
    uint8_t  i;
    uint16_t id;

    if (tc->app_data_len < 1u) {
        return PUS_ERR_BAD_LENGTH;
    }

    count = pus_get_u8(&tc->app_data[0]);
    if (tc->app_data_len != (size_t)(1u + ((size_t)count * 2u))) {
        return PUS_ERR_BAD_LENGTH;
    }
    if (count == 0u) {
        return PUS_ERR_BAD_PARAMETER;
    }

    for (i = 0u; i < count; ++i) {
        id = pus_get_u16(&tc->app_data[1u + ((size_t)i * 2u)]);
        if (svc05_id_valid(id) == PUS_FALSE) {
            return PUS_ERR_UNKNOWN_EVENT;
        }
    }

    for (i = 0u; i < count; ++i) {
        id = pus_get_u16(&tc->app_data[1u + ((size_t)i * 2u)]);
        s_enabled[id - 1u] = value;
    }

    return PUS_OK;
}

pus_status_t svc05_handle(const pus_tc_t *tc)
{
    pus_status_t st;

    if (tc == NULL) {
        return PUS_ERR_NULL;
    }

    switch (tc->service_subtype) {
    case PUS_ST05_DISABLE:
        st = svc05_set_enabled(tc, PUS_FALSE);
        break;
    case PUS_ST05_ENABLE:
        st = svc05_set_enabled(tc, PUS_TRUE);
        break;
    default:
        st = PUS_ERR_UNKNOWN_SUBTYPE;
        break;
    }

    return st;
}
