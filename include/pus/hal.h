/**
 * @file    hal.h
 * @brief   Thin abstraction isolating the flight software from the board.
 *
 * The application core links against this interface only. The flight build
 * provides a BSP implementation (timer + SpaceWire/UART driver); the host
 * build provides a deterministic implementation driven by the tick counter,
 * which is what makes the unit and integration tests reproducible.
 */
#ifndef PUS_HAL_H
#define PUS_HAL_H

#include "pus/pus_types.h"

/** Reset the platform time base to zero. */
void hal_time_init(void);

/** Advance the platform time base by one minor cycle. */
void hal_time_advance(void);

/**
 * @brief Read the on-board time as a CCSDS unsegmented time code.
 * @param coarse  Receives whole seconds since epoch.
 * @param fine    Receives the sub-second part in units of 1/65536 s.
 */
void hal_get_cuc_time(uint32_t *coarse, uint16_t *fine);

/** @brief Set the on-board time (PUS service 9 time management). */
void hal_set_cuc_time(uint32_t coarse, uint16_t fine);

/** @return Raw thermistor reading in units of 0.1 degree Celsius. */
int16_t hal_read_temperature(void);

/** @brief Test hook: force the value returned by hal_read_temperature(). */
void hal_inject_temperature(int16_t value_dc);

/** @brief Drive the survival heater line. */
void hal_set_heater(pus_bool_t on);

/** @return Current state of the survival heater line. */
pus_bool_t hal_get_heater(void);

#endif /* PUS_HAL_H */
