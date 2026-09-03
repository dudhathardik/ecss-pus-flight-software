/**
 * @file    svc03_housekeeping.h
 * @brief   PUS service 3 - housekeeping reporting.
 */
#ifndef PUS_SVC03_H
#define PUS_SVC03_H

#include "pus/pus_tc.h"

#define PUS_SERVICE_HOUSEKEEPING   3u
#define PUS_ST03_ENABLE            5u
#define PUS_ST03_DISABLE           6u
#define PUS_ST03_REPORT            25u
#define PUS_ST03_ONE_SHOT          27u

/** Structure identifier of the OBC essential housekeeping report. */
#define OBC_HK_SID_ESSENTIAL       1u
/** Structure identifier of the OBC diagnostic housekeeping report. */
#define OBC_HK_SID_DIAGNOSTIC      2u

/** Reset generation flags to their power-on values (essential enabled). */
void svc03_init(void);

/** Handle a service 3 telecommand. */
pus_status_t svc03_handle(const pus_tc_t *tc);

/** Emit a (3,25) report for @p sid. */
pus_status_t svc03_generate(uint8_t sid);

/** Called once per minor cycle; emits periodic reports when due. */
void svc03_tick(void);

/** @return PUS_TRUE when periodic generation is enabled for @p sid. */
pus_bool_t svc03_is_enabled(uint8_t sid);

#endif /* PUS_SVC03_H */
