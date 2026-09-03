/**
 * @file    ccsds.c
 * @brief   CCSDS 133.0-B space packet primary header codec.
 */
#include "pus/ccsds.h"
#include "pus/pus_bytes.h"

/**
 * @implements SWREQ-CCSDS-010
 * @implements SWREQ-ROB-010
 */
pus_status_t ccsds_pack_primary(const ccsds_primary_header_t *hdr,
                                uint8_t *buf, size_t buf_len)
{
    uint16_t word0;
    uint16_t word1;

    if ((hdr == NULL) || (buf == NULL)) {
        return PUS_ERR_NULL;
    }
    if (buf_len < CCSDS_PRIMARY_HEADER_LEN) {
        return PUS_ERR_BUFFER_TOO_SMALL;
    }

    word0 = (uint16_t)(((uint16_t)(hdr->version & 0x07u) << 13)      |
                       ((uint16_t)(hdr->packet_type & 0x01u) << 12)  |
                       ((uint16_t)(hdr->sec_hdr_flag & 0x01u) << 11) |
                       (uint16_t)(hdr->apid & 0x07FFu));

    word1 = (uint16_t)(((uint16_t)(hdr->seq_flags & 0x03u) << 14) |
                       (uint16_t)(hdr->seq_count & 0x3FFFu));

    pus_put_u16(&buf[0], word0);
    pus_put_u16(&buf[2], word1);
    pus_put_u16(&buf[4], hdr->data_length);

    return PUS_OK;
}

/** @implements SWREQ-CCSDS-010 */
pus_status_t ccsds_unpack_primary(const uint8_t *buf, size_t buf_len,
                                  ccsds_primary_header_t *hdr)
{
    uint16_t word0;
    uint16_t word1;

    if ((buf == NULL) || (hdr == NULL)) {
        return PUS_ERR_NULL;
    }
    if (buf_len < CCSDS_PRIMARY_HEADER_LEN) {
        return PUS_ERR_TRUNCATED;
    }

    word0 = pus_get_u16(&buf[0]);
    word1 = pus_get_u16(&buf[2]);

    hdr->version      = (uint8_t)((word0 >> 13) & 0x07u);
    hdr->packet_type  = (uint8_t)((word0 >> 12) & 0x01u);
    hdr->sec_hdr_flag = (uint8_t)((word0 >> 11) & 0x01u);
    hdr->apid         = (uint16_t)(word0 & 0x07FFu);
    hdr->seq_flags    = (uint8_t)((word1 >> 14) & 0x03u);
    hdr->seq_count    = (uint16_t)(word1 & 0x3FFFu);
    hdr->data_length  = pus_get_u16(&buf[4]);

    return PUS_OK;
}

/** @implements SWREQ-CCSDS-030 */
size_t ccsds_total_length(const ccsds_primary_header_t *hdr)
{
    if (hdr == NULL) {
        return 0u;
    }
    return (size_t)CCSDS_PRIMARY_HEADER_LEN + (size_t)hdr->data_length + 1u;
}
