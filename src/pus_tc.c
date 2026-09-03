/**
 * @file    pus_tc.c
 * @brief   Telecommand acceptance checks (ECSS-E-ST-70-41C clause 5.4.3).
 */
#include "pus/pus_tc.h"
#include "pus/pus_bytes.h"
#include "pus/pus_config.h"
#include "pus/crc16.h"

/** Smallest telecommand that can possibly be valid. */
#define PUS_TC_MIN_LEN  (CCSDS_PRIMARY_HEADER_LEN + PUS_TC_SEC_HEADER_LEN + CCSDS_CRC_LEN)

/**
 * @implements SWREQ-TC-010
 * @implements SWREQ-TC-015
 * @implements SWREQ-TC-020
 * @implements SWREQ-TC-030
 * @implements SWREQ-TC-040
 * @implements SWREQ-TC-050
 * @implements SWREQ-TC-060
 * @implements SWREQ-TC-070
 * @implements SWREQ-CCSDS-020
 * @implements SWREQ-CRC-020
 * @implements SWREQ-ROB-010
 *
 * The order of the checks is fixed by SWREQ-TC-015: nothing in the packet is
 * interpreted before the declared length is consistent and the packet error
 * control field verifies, so a corrupted APID can never be reported as an
 * addressing error when it is in fact a bit flip on the link.
 */
pus_status_t pus_tc_parse(const uint8_t *buf, size_t len, pus_tc_t *tc)
{
    pus_status_t st;
    size_t       total;
    uint16_t     computed;
    uint16_t     received;

    if ((buf == NULL) || (tc == NULL)) {
        return PUS_ERR_NULL;
    }
    if (len < (size_t)PUS_TC_MIN_LEN) {
        return PUS_ERR_TRUNCATED;
    }

    st = ccsds_unpack_primary(buf, len, &tc->prim);
    if (st != PUS_OK) {
        return st;
    }

    /* The framing layer delivers exactly one packet, so the declared length
       and the received length must agree. */
    total = ccsds_total_length(&tc->prim);
    if (total != len) {
        return PUS_ERR_BAD_LENGTH;
    }
    computed = crc16_ccitt(buf, total - (size_t)CCSDS_CRC_LEN);
    received = pus_get_u16(&buf[total - (size_t)CCSDS_CRC_LEN]);
    if (computed != received) {
        return PUS_ERR_BAD_CRC;
    }

    if (tc->prim.version != 0u) {
        return PUS_ERR_BAD_VERSION;
    }
    if (tc->prim.packet_type != (uint8_t)CCSDS_TYPE_TC) {
        return PUS_ERR_BAD_PACKET_TYPE;
    }
    if (tc->prim.sec_hdr_flag != 1u) {
        return PUS_ERR_NO_SEC_HEADER;
    }
    if (tc->prim.apid != (uint16_t)PUS_TC_APID) {
        return PUS_ERR_BAD_APID;
    }

    tc->pus_version     = (uint8_t)((buf[CCSDS_PRIMARY_HEADER_LEN] >> 4) & 0x0Fu);
    tc->ack_flags       = (uint8_t)(buf[CCSDS_PRIMARY_HEADER_LEN] & 0x0Fu);
    tc->service_type    = buf[CCSDS_PRIMARY_HEADER_LEN + 1u];
    tc->service_subtype = buf[CCSDS_PRIMARY_HEADER_LEN + 2u];
    tc->source_id       = pus_get_u16(&buf[CCSDS_PRIMARY_HEADER_LEN + 3u]);

    if (tc->pus_version != (uint8_t)PUS_VERSION_C) {
        return PUS_ERR_BAD_VERSION;
    }

    tc->app_data_len = total - (size_t)PUS_TC_MIN_LEN;
    tc->app_data     = (tc->app_data_len > 0u)
                     ? &buf[CCSDS_PRIMARY_HEADER_LEN + PUS_TC_SEC_HEADER_LEN]
                     : NULL;

    /** @implements SWREQ-SVC1-040 */
    tc->request_id = pus_get_u32(&buf[0]);

    return PUS_OK;
}

/** @implements SWREQ-TC-080 */
pus_bool_t pus_tc_ack_requested(const pus_tc_t *tc, uint8_t ack_mask)
{
    if (tc == NULL) {
        return PUS_FALSE;
    }
    return ((tc->ack_flags & ack_mask) != 0u) ? PUS_TRUE : PUS_FALSE;
}
