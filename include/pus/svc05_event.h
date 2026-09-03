/**
 * @file    svc05_event.h
 * @brief   PUS service 5 - event reporting.
 */
#ifndef PUS_SVC05_H
#define PUS_SVC05_H

#include "pus/pus_tc.h"

#define PUS_SERVICE_EVENT      5u
#define PUS_ST05_INFORMATIVE   1u
#define PUS_ST05_LOW_SEVERITY  2u
#define PUS_ST05_MED_SEVERITY  3u
#define PUS_ST05_HIGH_SEVERITY 4u
#define PUS_ST05_DISABLE       5u
#define PUS_ST05_ENABLE        6u

/** Event identifiers. The numeric values are part of the ground database. */
typedef enum {
    OBC_EVT_BOOT_COMPLETED   = 1, /**< Informative: application started.      */
    OBC_EVT_MODE_CHANGED     = 2, /**< Informative: aux = new mode.           */
    OBC_EVT_TC_REJECTED      = 3, /**< Low severity: aux = failure code.      */
    OBC_EVT_TM_QUEUE_OVERFLOW= 4, /**< Low severity: aux = drop count.        */
    OBC_EVT_HEATER_SWITCHED  = 5, /**< Informative: aux = new heater state.   */
    OBC_EVT_TEMP_ALARM       = 6  /**< High severity: aux = temperature.      */
} obc_event_id_t;

/** Re-enable every event identifier. */
void svc05_init(void);

/** Handle a service 5 telecommand. */
pus_status_t svc05_handle(const pus_tc_t *tc);

/**
 * @brief Report an event unless its identifier has been disabled.
 * @param id        Event identifier.
 * @param severity  One of PUS_ST05_INFORMATIVE .. PUS_ST05_HIGH_SEVERITY.
 * @param aux       Auxiliary data word appended to the report.
 */
pus_status_t svc05_report(obc_event_id_t id, uint8_t severity, uint16_t aux);

/** @return PUS_TRUE when reporting of @p id is enabled. */
pus_bool_t svc05_is_enabled(obc_event_id_t id);

#endif /* PUS_SVC05_H */
