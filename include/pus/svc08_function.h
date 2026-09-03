/**
 * @file    svc08_function.h
 * @brief   PUS service 8 - function management.
 */
#ifndef PUS_SVC08_H
#define PUS_SVC08_H

#include "pus/pus_tc.h"

#define PUS_SERVICE_FUNCTION    8u
#define PUS_ST08_PERFORM        1u

/** Function identifiers exposed to ground. */
typedef enum {
    OBC_FID_SET_MODE      = 1, /**< Argument: 1 octet, target OBC mode.       */
    OBC_FID_SET_HEATER    = 2, /**< Argument: 1 octet, 0 = off, 1 = on.       */
    OBC_FID_INJECT_TEMP   = 3, /**< Argument: 2 octets, signed 0.1 degC.      */
    OBC_FID_RESET_COUNTERS= 4, /**< No argument.                              */
    OBC_FID_SET_TIME      = 5  /**< Argument: 6 octets, CUC coarse + fine.    */
} obc_function_id_t;

/** Handle a service 8 telecommand. */
pus_status_t svc08_handle(const pus_tc_t *tc);

#endif /* PUS_SVC08_H */
