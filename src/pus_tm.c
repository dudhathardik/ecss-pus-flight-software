/**
 * @file    pus_tm.c
 * @brief   Telemetry packet construction (ECSS-E-ST-70-41C clause 5.4.2).
 */
#include "pus/pus_tm.h"
#include "pus/pus_bytes.h"
#include "pus/pus_config.h"
#include "pus/pus_tc.h"      /* PUS_VERSION_C */
#include "pus/crc16.h"
#include "pus/hal.h"
#include "pus/tm_queue.h"

/** Smallest telemetry packet: headers plus the error control field. */
#define PUS_TM_MIN_LEN \
    (CCSDS_PRIMARY_HEADER_LEN + PUS_TM_SEC_HEADER_LEN + CCSDS_CRC_LEN)

/** Spacecraft time reference status: 0 = time not synchronised to ground. */
#define PUS_TM_TIME_REF_STATUS  0u

/** Packet sequence count, shared by every APID emitted by this application. */
static uint16_t s_seq_count;

/**
 * Message type counter, one per service type.
 *
 * DEVIATION D-01 (see SRS.md): ECSS-E-ST-70-41C requires one counter per
 * (APID, service type, message subtype) triplet. A full table would cost
 * 128 KiB, which does not fit the target. The application emits a single APID
 * and the ground segment only uses the counter for gap detection per service,
 * so the counter is kept per service type (512 bytes). Agreed at the SRR.
 */
static uint16_t s_msg_counter[256];

/** @implements SWREQ-TM-020 */
void pus_tm_reset(void)
{
    size_t i;

    s_seq_count = 0u;
    for (i = 0u; i < PUS_ARRAY_LEN(s_msg_counter); ++i) {
        s_msg_counter[i] = 0u;
    }
}

/**
 * @implements SWREQ-TM-010
 * @implements SWREQ-TM-020
 * @implements SWREQ-TM-030
 * @implements SWREQ-TM-040
 * @implements SWREQ-TM-050
 * @implements SWREQ-ROB-010
 */
pus_status_t pus_tm_build(uint8_t *buf, size_t cap,
                          uint8_t service, uint8_t subtype,
                          const uint8_t *src_data, size_t src_len,
                          size_t *out_len)
{
    ccsds_primary_header_t hdr;
    pus_status_t st;
    size_t       total;
    size_t       i;
    size_t       off;
    uint32_t     coarse;
    uint16_t     fine;
    uint16_t     crc;

    if ((buf == NULL) || (out_len == NULL)) {
        return PUS_ERR_NULL;
    }
    if ((src_data == NULL) && (src_len > 0u)) {
        return PUS_ERR_NULL;
    }

    total = (size_t)PUS_TM_MIN_LEN + src_len;
    if ((total > cap) || (total > (size_t)PUS_MAX_PACKET_SIZE)) {
        return PUS_ERR_BUFFER_TOO_SMALL;
    }

    hdr.version      = 0u;
    hdr.packet_type  = (uint8_t)CCSDS_TYPE_TM;
    hdr.sec_hdr_flag = 1u;
    hdr.apid         = (uint16_t)PUS_TM_APID;
    hdr.seq_flags    = (uint8_t)CCSDS_SEQ_FLAG_UNSEGMENTED;
    hdr.seq_count    = (uint16_t)(s_seq_count & 0x3FFFu);
    hdr.data_length  = (uint16_t)(total - (size_t)CCSDS_PRIMARY_HEADER_LEN - 1u);

    st = ccsds_pack_primary(&hdr, buf, cap);
    if (st != PUS_OK) {
        return st;
    }

    off = (size_t)CCSDS_PRIMARY_HEADER_LEN;
    buf[off]      = (uint8_t)(((uint8_t)PUS_VERSION_C << 4) |
                              (uint8_t)PUS_TM_TIME_REF_STATUS);
    buf[off + 1u] = service;
    buf[off + 2u] = subtype;
    pus_put_u16(&buf[off + 3u], s_msg_counter[service]);
    pus_put_u16(&buf[off + 5u], (uint16_t)PUS_TM_DESTINATION_ID);

    hal_get_cuc_time(&coarse, &fine);
    pus_put_u32(&buf[off + 7u], coarse);
    pus_put_u16(&buf[off + 11u], fine);

    off += (size_t)PUS_TM_SEC_HEADER_LEN;
    for (i = 0u; i < src_len; ++i) {
        buf[off + i] = src_data[i];
    }
    off += src_len;

    crc = crc16_ccitt(buf, off);
    pus_put_u16(&buf[off], crc);

    s_seq_count = (uint16_t)((s_seq_count + 1u) & 0x3FFFu);
    s_msg_counter[service] = (uint16_t)(s_msg_counter[service] + 1u);

    *out_len = total;
    return PUS_OK;
}

/** @implements SWREQ-TM-060 */
pus_status_t pus_tm_send(uint8_t service, uint8_t subtype,
                         const uint8_t *src_data, size_t src_len)
{
    uint8_t      packet[PUS_MAX_PACKET_SIZE];
    size_t       len = 0u;
    pus_status_t st;

    st = pus_tm_build(packet, sizeof(packet), service, subtype,
                      src_data, src_len, &len);
    if (st != PUS_OK) {
        return st;
    }

    return tm_queue_push(packet, len);
}
