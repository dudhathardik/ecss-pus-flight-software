/**
 * @file    pus_types.h
 * @brief   Common types and status codes for the OBSW PUS stack.
 */
#ifndef PUS_TYPES_H
#define PUS_TYPES_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Status code returned by every fallible function of the stack.
 *
 * The numeric values are frozen: they are reported to ground inside
 * service 1 failure reports and must therefore not be reordered.
 */
typedef enum {
    PUS_OK                     = 0,  /**< Operation completed nominally.        */
    PUS_ERR_NULL               = 1,  /**< A mandatory pointer argument was NULL.*/
    PUS_ERR_BUFFER_TOO_SMALL   = 2,  /**< Output buffer cannot hold the result. */
    PUS_ERR_TRUNCATED          = 3,  /**< Packet shorter than its header claims.*/
    PUS_ERR_BAD_VERSION        = 4,  /**< CCSDS or PUS version number invalid.  */
    PUS_ERR_BAD_PACKET_TYPE    = 5,  /**< TM packet received on the TC port.    */
    PUS_ERR_NO_SEC_HEADER      = 6,  /**< Secondary header flag not set.        */
    PUS_ERR_BAD_APID           = 7,  /**< APID not routed to this application.  */
    PUS_ERR_BAD_CRC            = 8,  /**< Packet error control field mismatch.  */
    PUS_ERR_UNKNOWN_SERVICE    = 9,  /**< No handler for this service type.     */
    PUS_ERR_UNKNOWN_SUBTYPE    = 10, /**< No handler for this message subtype.  */
    PUS_ERR_BAD_LENGTH         = 11, /**< Application data length not as expected*/
    PUS_ERR_BAD_PARAMETER      = 12, /**< Parameter value outside its range.    */
    PUS_ERR_UNKNOWN_SID        = 13, /**< Housekeeping structure ID not defined.*/
    PUS_ERR_UNKNOWN_EVENT      = 14, /**< Event identifier not defined.         */
    PUS_ERR_UNKNOWN_FUNCTION   = 15, /**< Function identifier not defined.      */
    PUS_ERR_QUEUE_FULL         = 16, /**< Downlink queue overflow.              */
    PUS_ERR_NOT_PERMITTED      = 17  /**< Rejected in the current OBC mode.     */
} pus_status_t;

/** Boolean-like type used throughout the stack (avoids <stdbool.h> on old BSPs). */
typedef uint8_t pus_bool_t;

#define PUS_TRUE   ((pus_bool_t)1u)
#define PUS_FALSE  ((pus_bool_t)0u)

/** Convenience: number of elements of a statically sized array. */
#define PUS_ARRAY_LEN(a)  (sizeof(a) / sizeof((a)[0]))

#endif /* PUS_TYPES_H */
