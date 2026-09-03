/**
 * @file    svc01_verification.c
 * @brief   PUS service 1 - request verification.
 */
#include "pus/svc01_verification.h"
#include "pus/pus_bytes.h"
#include "pus/pus_tm.h"

/** Success reports carry the request ID only. */
#define SVC01_SUCCESS_DATA_LEN  4u
/** Failure reports append the 16-bit failure code. */
#define SVC01_FAILURE_DATA_LEN  6u

static pus_status_t svc01_send_success(uint8_t subtype, uint32_t request_id)
{
    uint8_t data[SVC01_SUCCESS_DATA_LEN];

    pus_put_u32(&data[0], request_id);
    return pus_tm_send((uint8_t)PUS_SERVICE_VERIFICATION, subtype,
                       data, sizeof(data));
}

static pus_status_t svc01_send_failure(uint8_t subtype, uint32_t request_id,
                                       pus_status_t code)
{
    uint8_t data[SVC01_FAILURE_DATA_LEN];

    pus_put_u32(&data[0], request_id);
    pus_put_u16(&data[4], (uint16_t)code);
    return pus_tm_send((uint8_t)PUS_SERVICE_VERIFICATION, subtype,
                       data, sizeof(data));
}

/** @implements SWREQ-SVC1-010 */
pus_status_t svc01_acceptance_success(const pus_tc_t *tc)
{
    if (tc == NULL) {
        return PUS_ERR_NULL;
    }
    if (pus_tc_ack_requested(tc, (uint8_t)PUS_ACK_ACCEPTANCE) == PUS_FALSE) {
        return PUS_OK;
    }
    return svc01_send_success((uint8_t)PUS_ST01_ACCEPTANCE_SUCCESS,
                              tc->request_id);
}

/** @implements SWREQ-SVC1-030 */
pus_status_t svc01_completion_success(const pus_tc_t *tc)
{
    if (tc == NULL) {
        return PUS_ERR_NULL;
    }
    if (pus_tc_ack_requested(tc, (uint8_t)PUS_ACK_COMPLETION) == PUS_FALSE) {
        return PUS_OK;
    }
    return svc01_send_success((uint8_t)PUS_ST01_COMPLETION_SUCCESS,
                              tc->request_id);
}

/** @implements SWREQ-SVC1-030 */
pus_status_t svc01_completion_failure(const pus_tc_t *tc, pus_status_t code)
{
    if (tc == NULL) {
        return PUS_ERR_NULL;
    }
    if (pus_tc_ack_requested(tc, (uint8_t)PUS_ACK_COMPLETION) == PUS_FALSE) {
        return PUS_OK;
    }
    return svc01_send_failure((uint8_t)PUS_ST01_COMPLETION_FAILURE,
                              tc->request_id, code);
}

/**
 * @implements SWREQ-SVC1-020
 * @implements SWREQ-SVC1-050
 *
 * A packet that fails the acceptance checks cannot be trusted to carry
 * meaningful acknowledgement flags, so the rejection is reported
 * unconditionally. When fewer than four octets arrived there is no request ID
 * to echo and zero is sent instead.
 */
pus_status_t svc01_reject_raw(const uint8_t *raw, size_t len, pus_status_t code)
{
    uint32_t request_id = 0u;

    if ((raw != NULL) && (len >= 4u)) {
        request_id = pus_get_u32(raw);
    }

    return svc01_send_failure((uint8_t)PUS_ST01_ACCEPTANCE_FAILURE,
                              request_id, code);
}
