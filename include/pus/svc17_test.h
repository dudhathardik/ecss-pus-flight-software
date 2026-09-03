/**
 * @file    svc17_test.h
 * @brief   PUS service 17 - connection test.
 */
#ifndef PUS_SVC17_H
#define PUS_SVC17_H

#include "pus/pus_tc.h"

#define PUS_SERVICE_TEST        17u
#define PUS_ST17_ARE_YOU_ALIVE  1u
#define PUS_ST17_I_AM_ALIVE     2u

/** Handle a service 17 telecommand. */
pus_status_t svc17_handle(const pus_tc_t *tc);

#endif /* PUS_SVC17_H */
