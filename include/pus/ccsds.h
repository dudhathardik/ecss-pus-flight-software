/**
 * @file    ccsds.h
 * @brief   CCSDS 133.0-B space packet primary header (6 octets).
 */
#ifndef PUS_CCSDS_H
#define PUS_CCSDS_H

#include "pus/pus_types.h"

/** Length of the CCSDS space packet primary header in octets. */
#define CCSDS_PRIMARY_HEADER_LEN   6u

/** Length of the packet error control (CRC) field in octets. */
#define CCSDS_CRC_LEN              2u

/** Packet type field values. */
#define CCSDS_TYPE_TM              0u
#define CCSDS_TYPE_TC              1u

/** Sequence flags: the stack only emits and accepts unsegmented packets. */
#define CCSDS_SEQ_FLAG_UNSEGMENTED 3u

/** Decoded CCSDS space packet primary header. */
typedef struct {
    uint8_t  version;       /**< Packet version number (3 bits), always 0.     */
    uint8_t  packet_type;   /**< 0 = TM, 1 = TC (1 bit).                       */
    uint8_t  sec_hdr_flag;  /**< Secondary header present (1 bit).             */
    uint16_t apid;          /**< Application process identifier (11 bits).     */
    uint8_t  seq_flags;     /**< Sequence flags (2 bits).                      */
    uint16_t seq_count;     /**< Packet sequence count (14 bits).              */
    uint16_t data_length;   /**< Octets in the data field minus one (16 bits). */
} ccsds_primary_header_t;

/**
 * @brief Serialise a primary header into @p buf.
 * @return PUS_OK, PUS_ERR_NULL or PUS_ERR_BUFFER_TOO_SMALL.
 */
pus_status_t ccsds_pack_primary(const ccsds_primary_header_t *hdr,
                                uint8_t *buf, size_t buf_len);

/**
 * @brief Deserialise a primary header from @p buf.
 * @return PUS_OK, PUS_ERR_NULL or PUS_ERR_TRUNCATED.
 */
pus_status_t ccsds_unpack_primary(const uint8_t *buf, size_t buf_len,
                                  ccsds_primary_header_t *hdr);

/**
 * @brief Total on-the-wire length of the packet described by @p hdr.
 * @return CCSDS_PRIMARY_HEADER_LEN + data_length + 1.
 */
size_t ccsds_total_length(const ccsds_primary_header_t *hdr);

#endif /* PUS_CCSDS_H */
