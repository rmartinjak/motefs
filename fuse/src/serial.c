
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>

#include "mfsmsg.h"
#include "mfsconst.h"
#include "sfsource.h"
#include "serialsource.h"
#include "serialpacket.h"


pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static enum { SRC_DEVICE, SRC_SF } src_type;
static serial_source src_dev;
static int src_fd;

#ifdef DEBUG
static void debug_packet(uint8_t *packet)
{
    unsigned i;
    for (i = 0; i < 1 + SPACKET_SIZE + MFSMSG_SIZE; i++)
    {
        if (isprint(packet[i]))
            fprintf(stderr, "%2c ", packet[i]);
        else
            fprintf(stderr, "%02x ", packet[i]);
    }
    fprintf(stderr, "\n");
}
#define DEBUG_PACKET(p) debug_packet(p)
#else
#define DEBUG_PACKET(p)
#endif

static char *msgs[] = {
    "unknown_packet_type",
    "ack_timeout",
    "sync",
    "too_long",
    "too_short",
    "bad_sync",
    "bad_crc",
    "closed",
    "no_memory",
    "unix_error"
};

static void stderr_msg(serial_source_msg problem)
{
    if (problem != 2)
        fprintf(stderr, "Note: %s\n", msgs[problem]);
}

static void set_data(tmsg_t *msg, const uint8_t *data, int len)
{
    int i;
    if (!len)
        return;
    for (i = 0; i < len; i++)
        mfsmsg_data_set(msg, i, data[i]);
}

static void get_data(tmsg_t *msg, uint8_t *data, int len)
{
    int i;
    if (!len)
        return;
    for (i = 0; i < len; i++)
        data[i] = mfsmsg_data_get(msg, i);
}

int serial_connect_dev(const char *dev, unsigned rate)
{
    src_dev = open_serial_source(dev, rate, 0, stderr_msg);
    if (!src_dev)
    {
        fprintf(stderr, "can't open device %s:%u\n", dev, rate);
        return -1;
    }
    src_type = SRC_DEVICE;
    return 0;
}

int serial_connect_sf(const char *host, unsigned port)
{
    src_fd = open_sf_source(host, port);
    if (src_fd < 0)
    {
        fprintf(stderr, "can't connect to serial forwarder %s:%u\n",
                host, port);
        return -1;
    }
    src_type = SRC_SF;
    return 0;
}

void serial_lock(void)
{
    pthread_mutex_lock(&mutex);
}

void serial_unlock(void)
{
    pthread_mutex_unlock(&mutex);
}

int serial_send(int node, int op, const uint8_t *data, int len)
{
    uint8_t buf[1 + SPACKET_SIZE + MFSMSG_SIZE];
    memset(buf, 0, sizeof buf);

    tmsg_t *spacket_header, *msg;

    buf[0] = 0;
    spacket_header = new_tmsg(buf + 1, SPACKET_SIZE);
    spacket_header_dest_set(spacket_header, 0xffff);
    spacket_header_length_set(spacket_header, MFSMSG_SIZE);
    spacket_header_type_set(spacket_header, MFSMSG_AM_TYPE);

    msg = new_tmsg(buf + 1 + SPACKET_SIZE, MFSMSG_SIZE);
    mfsmsg_node_set(msg, node);
    mfsmsg_op_set(msg, op);

    if (len > MFS_DATA_SIZE)
        len = MFS_DATA_SIZE;
    set_data(msg, data, len);

    DEBUG_PACKET(buf);

    return (src_type == SRC_DEVICE) ?
            write_serial_packet(src_dev, buf, sizeof buf) :
            write_sf_packet(src_fd, buf, sizeof buf);
}

int serial_receive(int *node, int *op, int *result, uint8_t *data, int len)
{
    uint8_t *buf;
    tmsg_t *msg;

    unsigned char *packet;
    int packet_len;

    packet = (src_type == SRC_DEVICE) ?
        read_serial_packet(src_dev, &packet_len) :
        read_sf_packet(src_fd, &packet_len);

    if (!packet)
        return -1;
    DEBUG_PACKET(packet);

    /* skip the header */
    buf = packet + 1 + SPACKET_SIZE;
    packet_len -= 1 + SPACKET_SIZE;

    if (packet_len > MFSMSG_SIZE)
    {
        fprintf(stderr, "packet too big: %d\n", len);
        packet_len = MFSMSG_SIZE;
    }

    msg = new_tmsg(buf, MFSMSG_SIZE);

    if (node)
        *node = mfsmsg_node_get(msg);
    if (op)
        *op = mfsmsg_op_get(msg);
    if (result)
        *result = mfsmsg_result_get(msg);

    if (len > packet_len)
        len = packet_len;

    get_data(msg, data, len);

    free(packet);
    return len;
}
