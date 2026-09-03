/**
 * @file    pus_config.h
 * @brief   Compile-time configuration of the OBSW PUS stack.
 *
 * All sizing constants live here so that the memory footprint of the
 * application is fully determined at compile time (no dynamic allocation,
 * see CODING_STANDARD.md rule R-MEM-01).
 */
#ifndef PUS_CONFIG_H
#define PUS_CONFIG_H

/* ------------------------------------------------------------------ */
/* Packet sizing                                                       */
/* ------------------------------------------------------------------ */

/** Maximum size of a complete CCSDS space packet (octets). */
#define PUS_MAX_PACKET_SIZE      256u

/** Depth of the downlink telemetry queue (number of packets). */
#define PUS_TM_QUEUE_DEPTH       16u

/* ------------------------------------------------------------------ */
/* Identification                                                      */
/* ------------------------------------------------------------------ */

/** APID accepted on the uplink (telecommands addressed to this OBC). */
#define PUS_TC_APID              0x00Cu
/** APID used on the downlink (telemetry produced by this OBC). */
#define PUS_TM_APID              0x00Cu
/** Destination ID inserted into every TM secondary header. */
#define PUS_TM_DESTINATION_ID    0x0001u

/* ------------------------------------------------------------------ */
/* Application configuration                                           */
/* ------------------------------------------------------------------ */

/** Duration of one OBSW minor cycle in milliseconds. */
#define OBC_TICK_PERIOD_MS       100u

/** Number of minor cycles between two periodic housekeeping reports. */
#define OBC_HK_PERIOD_TICKS      10u

/** Number of housekeeping structures supported. */
#define OBC_HK_MAX_STRUCTURES    2u

/** Number of distinct event identifiers supported. */
#define OBC_EVENT_MAX_IDS        8u

/* ------------------------------------------------------------------ */
/* Thermal control thresholds (units: 0.1 degree Celsius)              */
/* ------------------------------------------------------------------ */

/** At or below this temperature the heater is switched on. */
#define OBC_TEMP_HEATER_ON_DC    50    /*   5.0 degC */
/** At or above this temperature the heater is switched off. */
#define OBC_TEMP_HEATER_OFF_DC   250   /*  25.0 degC */
/** At or above this temperature the OBC enters SAFE mode. */
#define OBC_TEMP_ALARM_DC        400   /*  40.0 degC */

#endif /* PUS_CONFIG_H */
