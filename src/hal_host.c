/**
 * @file    hal_host.c
 * @brief   Host implementation of the hardware abstraction layer.
 *
 * Time is derived from the minor cycle counter rather than from the wall
 * clock: every test therefore observes exactly the same time stamps on every
 * run, which is what allows the integration tests to compare whole packets
 * instead of only selected fields.
 *
 * The flight build replaces this file with hal_gr712.c; nothing above the HAL
 * changes.
 */
#include "pus/hal.h"
#include "pus/pus_config.h"

static uint32_t   s_ticks;
static uint32_t   s_epoch_coarse;
static uint16_t   s_epoch_fine;
static int16_t    s_temperature_dc;
static pus_bool_t s_heater_on;

/** @implements SWREQ-HAL-010 */
void hal_time_init(void)
{
    s_ticks          = 0u;
    s_epoch_coarse   = 0u;
    s_epoch_fine     = 0u;
    s_temperature_dc = 200; /* 20.0 degC, mid-range at power-on */
    s_heater_on      = PUS_FALSE;
}

void hal_time_advance(void)
{
    s_ticks += 1u;
}

/** @implements SWREQ-HAL-010 */
void hal_get_cuc_time(uint32_t *coarse, uint16_t *fine)
{
    uint32_t elapsed_ms;
    uint32_t total_fine;

    if ((coarse == NULL) || (fine == NULL)) {
        return;
    }

    elapsed_ms = s_ticks * (uint32_t)OBC_TICK_PERIOD_MS;

    /* 1/65536 s units; 65536 / 1000 is kept as a rational factor to avoid
       floating point in flight code (CODING_STANDARD.md rule R-NUM-02). */
    total_fine = (uint32_t)s_epoch_fine +
                 (((elapsed_ms % 1000u) * 65536u) / 1000u);

    *coarse = s_epoch_coarse + (elapsed_ms / 1000u) + (total_fine / 65536u);
    *fine   = (uint16_t)(total_fine % 65536u);
}

/** @implements SWREQ-HAL-020 */
void hal_set_cuc_time(uint32_t coarse, uint16_t fine)
{
    uint32_t elapsed_ms = s_ticks * (uint32_t)OBC_TICK_PERIOD_MS;

    /* Store the epoch so that the time reported right now equals the value
       just commanded, and keeps advancing with the tick counter afterwards. */
    s_epoch_coarse = coarse - (elapsed_ms / 1000u);
    s_epoch_fine   = (uint16_t)(fine -
                     (uint16_t)(((elapsed_ms % 1000u) * 65536u) / 1000u));
}

int16_t hal_read_temperature(void)
{
    return s_temperature_dc;
}

void hal_inject_temperature(int16_t value_dc)
{
    s_temperature_dc = value_dc;
}

void hal_set_heater(pus_bool_t on)
{
    s_heater_on = (on != PUS_FALSE) ? PUS_TRUE : PUS_FALSE;
}

pus_bool_t hal_get_heater(void)
{
    return s_heater_on;
}
