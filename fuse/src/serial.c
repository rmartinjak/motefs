
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>

#include "mfsmsg.h"
#include "mfsconst.h"
#include "serialsource.h"
#include "serialpacket.h"


pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static serial_source serial_src;

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


int serial_connect(const char *dev, unsigned rate)
{
    serial_src = open_serial_source(dev, rate, 0, stderr_msg);

    if (!serial_src)
        return -1;
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

    return write_serial_packet(serial_src, buf, sizeof buf);
}

int serial_receive(int *node, int *op, int *result, uint8_t *data, int len)
{
    uint8_t buf[MFSMSG_SIZE];
    tmsg_t *msg;

    unsigned char *packet;
    int packet_len;

    packet = read_serial_packet(serial_src, &packet_len);

    if (!packet)
        return -1;

    /* skip the header */
    packet += 1 + SPACKET_SIZE;
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