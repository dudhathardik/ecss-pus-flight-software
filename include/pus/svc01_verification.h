/**
 * @file    svc01_verification.h
 * @brief   PUS service 1 - request verification.
 */
#ifndef PUS_SVC01_H
#define PUS_SVC01_H

#include "pus/pus_tc.h"

#define PUS_SERVICE_VERIFICATION      1u
#define PUS_ST01_ACCEPTANCE_SUCCESS   1u
#define PUS_ST01_ACCEPTANCE_FAILURE   2u
#define PUS_ST01_COMPLETION_SUCCESS   7u
#define PUS_ST01_COMPLETION_FAILURE   8u

/** Emit (1,1) if the telecommand requested an acceptance acknowledgement. */
pus_status_t svc01_acceptance_success(const pus_tc_t *tc);

/** Emit (1,7) if the telecommand requested a completion acknowledgement. */
pus_status_t svc01_completion_success(const pus_tc_t *tc);

/** Emit (1,8) if the telecommand requested a completion acknowledgement. */
pus_status_t svc01_completion_failure(const pus_tc_t *tc, pus_status_t code);

/**
 * @brief Emit (1,2) for a telecommand that failed its acceptance checks.
 *
 * Acceptance failures are reported from the raw octets rather than from a
 * decoded packet: a telecommand that failed the checks has, by definition, no
 * trustworthy decoded form.
 * @param raw  Raw packet octets; the request ID is recovered from the first
 *             four octets when at least four are available, else zero.
 */
pus_status_t svc01_reject_raw(const uint8_t *raw, size_t len, pus_status_t code);

#endif /* PUS_SVC01_H */
