/**
 * @file    test_support.c
 * @brief   Shared helpers for the unit test cases.
 */
#include "test_support.h"

#include "pus/ccsds.h"
#include "pus/crc16.h"
#include "pus/pus_bytes.h"
#include "pus/pus_tm.h"
#include "pus/obc_app.h"
#include "pus/tm_queue.h"

size_t ts_build_tc(uint8_t *buf, size_t cap,
                   uint8_t service, uint8_t subtype, uint8_t ack_flags,
                   const uint8_t *app_data, size_t app_len)
{
    ccsds_primary_header_t hdr;
    size_t total;
    size_t i;
    size_t off;

    total = (size_t)CCSDS_PRIMARY_HEADER_LEN + (size_t)PUS_TC_SEC_HEADER_LEN +
            app_len + (size_t)CCSDS_CRC_LEN;
    if (total > cap) {
        return 0u;
    }

    hdr.version      = 0u;
    hdr.packet_type  = (uint8_t)CCSDS_TYPE_TC;
    hdr.sec_hdr_flag = 1u;
    hdr.apid         = (uint16_t)PUS_TC_APID;
    hdr.seq_flags    = (uint8_t)CCSDS_SEQ_FLAG_UNSEGMENTED;
    hdr.seq_count    = 1u;
    hdr.data_length  = (uint16_t)(total - (size_t)CCSDS_PRIMARY_HEADER_LEN - 1u);
    (void)ccsds_pack_primary(&hdr, buf, cap);

    off = (size_t)CCSDS_PRIMARY_HEADER_LEN;
    buf[off]      = (uint8_t)(((uint8_t)PUS_VERSION_C << 4) | (ack_flags & 0x0Fu));
    buf[off + 1u] = service;
    buf[off + 2u] = subtype;
    pus_put_u16(&buf[off + 3u], 0x0101u);

    off += (size_t)PUS_TC_SEC_HEADER_LEN;
    for (i = 0u; i < app_len; ++i) {
        buf[off + i] = app_data[i];
    }

    ts_fix_crc(buf, total);
    return total;
}

void ts_fix_crc(uint8_t *buf, size_t len)
{
    uint16_t crc = crc16_ccitt(buf, len - (size_t)CCSDS_CRC_LEN);
    pus_put_u16(&buf[len - (size_t)CCSDS_CRC_LEN], crc);
}

void ts_break_crc(uint8_t *buf, size_t len)
{
    buf[len - 1u] ^= 0x01u;
}

pus_bool_t ts_tm_pop(uint8_t *buf, size_t cap, ts_tm_t *tm)
{
    size_t len = 0u;
    ccsds_primary_header_t hdr;
    size_t off;

    if (tm_queue_pop(buf, cap, &len) != PUS_OK) {
        return PUS_FALSE;
    }
    (void)ccsds_unpack_primary(buf, len, &hdr);

    off = (size_t)CCSDS_PRIMARY_HEADER_LEN;
    tm->seq_count   = hdr.seq_count;
    tm->service     = buf[off + 1u];
    tm->subtype     = buf[off + 2u];
    tm->msg_counter = pus_get_u16(&buf[off + 3u]);
    tm->time_coarse = pus_get_u32(&buf[off + 7u]);
    tm->time_fine   = pus_get_u16(&buf[off + 11u]);
    tm->total_len   = len;
    tm->data_len    = len - (size_t)CCSDS_PRIMARY_HEADER_LEN -
                      (size_t)PUS_TM_SEC_HEADER_LEN - (size_t)CCSDS_CRC_LEN;
    tm->data        = &buf[off + PUS_TM_SEC_HEADER_LEN];

    return PUS_TRUE;
}

pus_bool_t ts_tm_find(uint8_t service, uint8_t subtype,
                      uint8_t *buf, size_t cap, ts_tm_t *tm)
{
    while (ts_tm_pop(buf, cap, tm) == PUS_TRUE) {
        if ((tm->service == service) && (tm->subtype == subtype)) {
            return PUS_TRUE;
        }
    }
    return PUS_FALSE;
}

void ts_tm_flush(void)
{
    uint8_t scratch[PUS_MAX_PACKET_SIZE];
    size_t  len;

    while (tm_queue_pop(scratch, sizeof(scratch), &len) == PUS_OK) {
        /* discard */
    }
}

pus_status_t ts_send_tc(uint8_t service, uint8_t subtype,
                        const uint8_t *app_data, size_t app_len)
{
    uint8_t tc[PUS_MAX_PACKET_SIZE];
    size_t  len;

    len = ts_build_tc(tc, sizeof(tc), service, subtype,
                      (uint8_t)PUS_ACK_ACCEPTANCE | (uint8_t)PUS_ACK_COMPLETION,
                      app_data, app_len);
    if (len == 0u) {
        return PUS_ERR_BUFFER_TOO_SMALL;
    }

    return obc_process_tc(tc, len);
}
