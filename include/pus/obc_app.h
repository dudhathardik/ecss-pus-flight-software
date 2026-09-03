/**
 * @file    obc_app.h
 * @brief   Application layer: mode management, thermal control, TC routing.
 */
#ifndef OBC_APP_H
#define OBC_APP_H

#include "pus/pus_tc.h"

/** On-board computer operating modes. */
typedef enum {
    OBC_MODE_SAFE    = 0, /**< Survival mode: heater inhibited.               */
    OBC_MODE_NOMINAL = 1  /**< Nominal mode: autonomous thermal control on.   */
} obc_mode_t;

/** Observable application state, mirrored in the housekeeping reports. */
typedef struct {
    obc_mode_t mode;              /**< Current operating mode.                */
    uint32_t   uptime_ticks;      /**< Minor cycles since obc_init().         */
    int16_t    temperature_dc;    /**< Last acquired temperature, 0.1 degC.   */
    pus_bool_t heater_on;         /**< Commanded heater state.                */
    uint16_t   tc_accepted;       /**< Telecommands accepted since reset.     */
    uint16_t   tc_rejected;       /**< Telecommands rejected since reset.     */
    uint16_t   last_failure_code; /**< Status code of the last rejection.     */
} obc_state_t;

/** Bring the application to its power-on state and emit the boot event. */
void obc_init(void);

/**
 * @brief Accept, route and execute one raw telecommand.
 *
 * Emits the service 1 acceptance and completion reports demanded by the
 * acknowledgement flags of @p buf.
 *
 * @return PUS_OK when the command completed, otherwise the failure code that
 *         was reported to ground.
 */
pus_status_t obc_process_tc(const uint8_t *buf, size_t len);

/** Execute one minor cycle: acquire, control, report. */
void obc_tick(void);

/** @return Read-only view of the application state. */
const obc_state_t *obc_get_state(void);

/** @brief Request a mode transition (service 8 function 1). */
pus_status_t obc_set_mode(obc_mode_t mode);

/**
 * @brief Command the survival heater from ground (service 8 function 2).
 * @return PUS_ERR_NOT_PERMITTED while the unit is in SAFE mode.
 */
pus_status_t obc_set_heater(pus_bool_t on);

/** @brief Reset the accepted/rejected telecommand counters. */
void obc_reset_counters(void);

#endif /* OBC_APP_H */
