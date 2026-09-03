/**
 * @file    obc_sim.c
 * @brief   Host runner: exposes the OBSW over a TCP socket as a TC/TM port.
 *
 * This file is test infrastructure, not flight software. It plays the role
 * the SpaceWire or UART driver plays on the target: it delivers one complete
 * telecommand at a time to obc_process_tc() and drains the downlink queue.
 * Nothing below sim/ is linked into the flight build.
 *
 * The on-board software is re-initialised on every new connection, so each
 * test case starts from a known state without restarting the process.
 *
 * Usage: obc_sim [--port N] [--tick-ms N] [--quiet]
 */
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>

#include "pus/ccsds.h"
#include "pus/obc_app.h"
#include "pus/pus_config.h"
#include "pus/tm_queue.h"

static int s_verbose = 1;

static void log_line(const char *fmt, ...)
{
    va_list ap;

    if (s_verbose == 0) {
        return;
    }
    va_start(ap, fmt);
    (void)vfprintf(stderr, fmt, ap);
    va_end(ap);
    (void)fflush(stderr);
}

/** Read exactly @p n octets, or return 0 on close / -1 on error. */
static int read_exact(int fd, uint8_t *buf, size_t n)
{
    size_t  got = 0u;
    ssize_t r;

    while (got < n) {
        r = recv(fd, &buf[got], n - got, 0);
        if (r == 0) {
            return 0;
        }
        if (r < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        got += (size_t)r;
    }
    return 1;
}

static int write_all(int fd, const uint8_t *buf, size_t n)
{
    size_t  sent = 0u;
    ssize_t w;

    while (sent < n) {
        w = send(fd, &buf[sent], n - sent, 0);
        if (w <= 0) {
            if ((w < 0) && (errno == EINTR)) {
                continue;
            }
            return -1;
        }
        sent += (size_t)w;
    }
    return 1;
}

/** Move everything currently queued for downlink onto the socket. */
static int drain_downlink(int fd)
{
    uint8_t packet[PUS_MAX_PACKET_SIZE];
    size_t  len;

    while (tm_queue_count() > 0u) {
        if (tm_queue_pop(packet, sizeof(packet), &len) != PUS_OK) {
            break;
        }
        if (write_all(fd, packet, len) < 0) {
            return -1;
        }
        log_line("  TM -> service %u subtype %u (%zu octets)\n",
                 (unsigned)packet[7], (unsigned)packet[8], len);
    }
    return 0;
}

/**
 * Receive one telecommand. The CCSDS primary header carries the length of the
 * rest of the packet, so no additional framing is needed on the link.
 *
 * @implements SWREQ-ROB-050
 */
static int receive_tc(int fd, uint8_t *buf, size_t cap, size_t *out_len)
{
    ccsds_primary_header_t hdr;
    size_t total;
    int    rc;

    rc = read_exact(fd, buf, (size_t)CCSDS_PRIMARY_HEADER_LEN);
    if (rc <= 0) {
        return rc;
    }

    (void)ccsds_unpack_primary(buf, (size_t)CCSDS_PRIMARY_HEADER_LEN, &hdr);
    total = ccsds_total_length(&hdr);

    if ((total > cap) || (total <= (size_t)CCSDS_PRIMARY_HEADER_LEN)) {
        /* Malformed length: hand the header alone to the acceptance checks,
           which will reject it and emit the (1,2) report. */
        *out_len = (size_t)CCSDS_PRIMARY_HEADER_LEN;
        return 1;
    }

    rc = read_exact(fd, &buf[CCSDS_PRIMARY_HEADER_LEN],
                    total - (size_t)CCSDS_PRIMARY_HEADER_LEN);
    if (rc <= 0) {
        return rc;
    }

    *out_len = total;
    return 1;
}

static void serve_client(int fd, unsigned tick_ms)
{
    uint8_t        buf[PUS_MAX_PACKET_SIZE];
    size_t         len;
    fd_set         rfds;
    struct timeval tv;
    int            rc;

    obc_init();
    log_line("-- connection accepted, OBSW re-initialised\n");

    if (drain_downlink(fd) < 0) {
        return;
    }

    for (;;) {
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        tv.tv_sec  = (time_t)(tick_ms / 1000u);
        tv.tv_usec = (suseconds_t)((tick_ms % 1000u) * 1000u);

        rc = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        if (rc == 0) {
            obc_tick();
        } else {
            rc = receive_tc(fd, buf, sizeof(buf), &len);
            if (rc <= 0) {
                break;
            }
            log_line("  TC <- %zu octets\n", len);
            (void)obc_process_tc(buf, len);
        }

        if (drain_downlink(fd) < 0) {
            break;
        }
    }

    log_line("-- connection closed after %u minor cycles\n",
             (unsigned)obc_get_state()->uptime_ticks);
}

int main(int argc, char **argv)
{
    int                port     = 12345;
    unsigned           tick_ms  = OBC_TICK_PERIOD_MS;
    int                listen_fd;
    int                client_fd;
    int                opt = 1;
    int                i;
    struct sockaddr_in addr;

    for (i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "--port") == 0) && ((i + 1) < argc)) {
            port = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "--tick-ms") == 0) && ((i + 1) < argc)) {
            tick_ms = (unsigned)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--quiet") == 0) {
            s_verbose = 0;
        } else {
            (void)fprintf(stderr,
                          "usage: %s [--port N] [--tick-ms N] [--quiet]\n",
                          argv[0]);
            return 2;
        }
    }

    (void)signal(SIGPIPE, SIG_IGN);

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return 1;
    }
    (void)setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port        = htons((uint16_t)port);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        (void)close(listen_fd);
        return 1;
    }
    if (listen(listen_fd, 1) < 0) {
        perror("listen");
        (void)close(listen_fd);
        return 1;
    }

    /* The integration test harness waits for this line before connecting. */
    (void)printf("obc_sim listening on 127.0.0.1:%d (tick %u ms)\n",
                 port, tick_ms);
    (void)fflush(stdout);

    for (;;) {
        client_fd = accept(listen_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("accept");
            break;
        }
        (void)setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
        serve_client(client_fd, tick_ms);
        (void)close(client_fd);
    }

    (void)close(listen_fd);
    return 0;
}
