#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "mfsconst.h"
#include "mfsmsg.h"
#include "serialsource.h"
#include "message.h"

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

void stderr_msg(serial_source_msg problem)
{
    fprintf(stderr, "Note: %s\n", msgs[problem]);
}

int main(int argc, char **argv)
{
    serial_source src;


    if (argc != 3)
    {
        fprintf(stderr,
                "Usage: %s <device> <rate> - dump packets from a serial port\n",
                argv[0]);
        exit(2);
    }
    src =
        open_serial_source(argv[1], platform_baud_rate(argv[2]), 0,
                           stderr_msg);
    if (!src)
    {
        fprintf(stderr, "Couldn't open serial port at %s:%s\n",
                argv[1], argv[2]);
        exit(1);
    }

    {
        int res;
        unsigned char data[MFSMSG_SIZE];
        tmsg_t *outmsg = new_tmsg(data, sizeof data);
        mfsmsg_op_set(outmsg, MFS_LIST);
        res = write_serial_packet(src, data, sizeof data);

        if (res >= 0)
        {
            puts("SUCCESS");
            if (res == 1)
                puts("but without ACK");
        }
        else
            puts("FAIL");
    }
    for (;;)
    {
        int len, i, c;
        tmsg_t *tmsg;
        unsigned char copy[1024];
        const unsigned char *packet = read_serial_packet(src, &len);

        if (!packet)
            exit(0);

        memcpy(copy, packet, len);

        tmsg = new_tmsg(copy, len);

        for (i = 0; (c = mfsmsg_data_get(tmsg, i)); i++)
            putchar(c);
        putchar('\n');

        for (i = 0; i < len; i++)
        {
            if (isprint(packet[i]))
                printf("%2c ", packet[i]);
            else
                printf("%02x ", packet[i]);
        }
        putchar('\n');
        putchar('\n');


        free((void *) packet);
    }
}
