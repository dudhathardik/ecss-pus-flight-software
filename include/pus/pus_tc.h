/**
 * @file    pus_tc.h
 * @brief   Parsing and validation of PUS-C telecommand packets.
 */
#ifndef PUS_TC_H
#define PUS_TC_H

#include "pus/ccsds.h"

/** Length of the TC packet secondary header in octets. */
#define PUS_TC_SEC_HEADER_LEN   5u

/** PUS version number carried by every packet of issue C. */
#define PUS_VERSION_C           2u

/** Acknowledgement flag bit masks (TC secondary header, 4 bits). */
#define PUS_ACK_ACCEPTANCE      0x1u
#define PUS_ACK_START           0x2u
#define PUS_ACK_PROGRESS        0x4u
#define PUS_ACK_COMPLETION      0x8u

/** Decoded telecommand packet. */
typedef struct {
    ccsds_primary_header_t prim;        /**< CCSDS primary header.             */
    uint8_t   pus_version;              /**< TC packet PUS version (4 bits).   */
    uint8_t   ack_flags;                /**< Requested acknowledgements.       */
    uint8_t   service_type;             /**< PUS service type.                 */
    uint8_t   service_subtype;          /**< PUS message subtype.              */
    uint16_t  source_id;                /**< Originating ground application.   */
    const uint8_t *app_data;            /**< Application data (may be NULL).   */
    size_t    app_data_len;             /**< Application data length in octets.*/
    uint32_t  request_id;               /**< First 4 octets of the primary hdr.*/
} pus_tc_t;

/**
 * @brief Parse and fully validate a raw telecommand.
 *
 * Checks, in this order: buffer size, CCSDS version, packet type, secondary
 * header flag, declared length consistency, APID routing, PUS version and
 * finally the packet error control field.
 *
 * @param buf  Raw packet octets.
 * @param len  Number of octets available in @p buf.
 * @param tc   Destination structure; @c app_data points into @p buf.
 * @return PUS_OK or the first validation error encountered.
 */
pus_status_t pus_tc_parse(const uint8_t *buf, size_t len, pus_tc_t *tc);

/** @brief Test whether the ground requested a given acknowledgement. */
pus_bool_t pus_tc_ack_requested(const pus_tc_t *tc, uint8_t ack_mask);

#endif /* PUS_TC_H */
