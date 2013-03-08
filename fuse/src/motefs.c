#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <errno.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <fuse_opt.h>
#include <sys/stat.h>

#include "serialsource.h"

#include "pack.h"
#include "mfsmsg.h"
#include "mfsconst.h"


static uint8_t node_count = 0;
static struct motefs_node
{
    char name[MFS_DATA_SIZE];
    uint8_t type;
    uint8_t op;
} nodes[256];
#define MFS_ISREAD(node) ((node).type == MFS_READ || (node).type == MFS_READWRITE)
#define MFS_ISWRITE(node) ((node).type == MFS_WRITE || (node).type == MFS_READWRITE)


static char serial_device[1024];
static bool serial_device_set = false;
static int serial_baudrate;

static serial_source serial_src;
static int serial_send(int node, int op, uint8_t *data, int len)
{
    int i;
    unsigned char buf[MFSMSG_SIZE];

    tmsg_t *msg = new_tmsg(buf, sizeof buf);

    if (len > MFS_DATA_SIZE)
        len = MFS_DATA_SIZE;

    mfsmsg_node_set(msg, node);
    mfsmsg_op_set(msg, op);

    for (i = 0; i < len; i++)
        mfsmsg_data_set(msg, i, data[i]);
    return 0;
}

static int get_node(const char *name)
{
    unsigned i;
    if (*name == '/')
        name++;

    if (!*name)
        return -1;

    for (i = 0; i < node_count; i++)
        if (!strcmp(name, nodes[i].name))
            return i;
    return -1;
}


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


static int op_getattr(const char *path, struct stat *stbuf)
{
    int n;
    memset(stbuf, 0, sizeof *stbuf);

    /* root directory */
    if (!strcmp(path, "/"))
    {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = node_count;
        return 0;
    }

    /* everything else */
    n = get_node(path);

    if (n == -1)
        return -ENOENT;

    stbuf->st_ino = n;
    stbuf->st_nlink = 1;

    stbuf->st_mode = S_IFREG;
    /* permission bits */
    switch (nodes[n].op)
    {
        case MFS_READ:
            stbuf->st_mode |= 0444;
            break;

        case MFS_WRITE:
            stbuf->st_mode |= 0200;
            break;

        case MFS_READWRITE:
            stbuf->st_mode |= 0644;
            break;
    }

    /* size */
    switch (nodes[n].type)
    {
        case MFS_BOOL:
            stbuf->st_size = 1;
            break;

        case MFS_INT:
            stbuf->st_size = sizeof(int64_t);
            break;

        case MFS_STR:
            stbuf->st_size = MFSMSG_SIZE;
            break;
    }

    return 0;
}

static int op_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi)
{
    (void) offset;
    (void) fi;

    unsigned i;

    if (strcmp(path, "/"))
        return -ENOENT;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    for (i = 0; i < node_count; i++)
    {
        filler(buf, nodes[i].name, NULL, 0);
    }
    return 0;
}

static int op_open(const char *path, struct fuse_file_info *fi)
{
    int n;

    n = get_node(path);

    if (n == -1)
        return -ENOENT;

    if (fi->flags & O_RDONLY && MFS_ISREAD(nodes[n]))
        return 0;

    if (fi->flags & O_WRONLY && MFS_ISWRITE(nodes[n]))
        return 0;

    return -EACCES;
}

static int op_read(const char *path, char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi)
{
    (void) fi;
    (void) offset;

    int n, res;
    uint8_t data[MFS_DATA_SIZE];

    n = get_node(path);

    if (n == -1)
        return -ENOENT;

    if (size < MFSMSG_SIZE)
        return -EIO;

    res = serial_send(n, MFS_READ, data, sizeof data);

    if (res == -1)
        return -EIO;

    switch (nodes[n].type)
    {
        case MFS_BOOL:
            strcpy(buf, (*data) ? "true" : "false");
            break;

        case MFS_INT:
            {
                int64_t val;
                unpack(data, "l", &val);
                sprintf(buf, "%" PRIi64, val);
            }
            break;

        case MFS_STR:
            memcpy(buf, data, res - 1);
            buf[res - 1] = '\0';
            break;
    }
    res = strlen(buf);
    return res;
}

static int op_write(const char *path, const char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi)
{
    (void) fi;
    (void) offset;

    int n, res = 0;
    uint8_t data[MFS_DATA_SIZE];

    n = get_node(path);

    if (n == -1)
        return -ENOENT;

    if (nodes[n].type == MFS_BOOL)
    {
#define MIN(a, b) (a < b) ? a : b
        if (!strncmp(buf, "true", MIN(sizeof "true", size)))
            data[0] = 1;
        else if (!strncmp(buf, "false", MIN(sizeof "false", size)))
            data[0] = 0;
#undef MIN
        else
            data[0] = ! !atoi(buf);
        size = 1;
    }
    else if (nodes[n].type == MFS_INT)
    {
        int64_t val = atoll(buf);
        size = pack(data, "l", val);
    }
    else if (nodes[n].type == MFS_STR)
    {
        unsigned i;
        if (size >= MFS_DATA_SIZE)
            size = MFS_DATA_SIZE - 1;

        for (i = 0; i < size; i++)
            data[i] = buf[i];
        data[size] = '\0';
    }

    res = serial_send(n, MFS_WRITE, data, size);
    if (res == -1)
        return -EIO;
    return size;
}

static struct fuse_operations motefs_ops = {
    .getattr = op_getattr,
    .readdir = op_readdir,
    .open = op_open,
    .read = op_read,
    .write = op_write,
};


enum motefs_opt_key
{
    MOTEFS_OPT_BAUDRATE,
};

static struct fuse_opt motefs_opts[] = {
    FUSE_OPT_KEY("rate=%s", MOTEFS_OPT_BAUDRATE),
    FUSE_OPT_END,
};

static int motefs_opt_proc(void *data, const char *arg, int key,
                           struct fuse_args *outargs)
{
    (void) data;
    (void) outargs;

    switch (key)
    {
        case FUSE_OPT_KEY_NONOPT:
            /* serial device is already set, pass the arg on to fuse */
            if (serial_device_set)
                return 1;
            puts(arg);
            strncpy(serial_device, arg, sizeof serial_device - 1);
            serial_device_set = true;
            return 0;

        case MOTEFS_OPT_BAUDRATE:
            if (!strchr(arg, '=') ||
                !(serial_baudrate = atoi(strchr(arg, '=') + 1)))
            {
                fprintf(stderr, "invalid baud rate: %s\n", arg);
                return -1;
            }
            return 0;
    }
    /* key unknown */
    return 1;
}


int main(int argc, char **argv)
{
    int res;
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

    if (fuse_opt_parse(&args, NULL, motefs_opts, motefs_opt_proc) == -1)
        return EXIT_FAILURE;

    serial_src =
        open_serial_source(serial_device, serial_baudrate, 0, stderr_msg);
    if (!serial_src)
    {
        fprintf(stderr, "can't open device %s\n", serial_device);
        return EXIT_FAILURE;
    }

    /* add "use_ino" to display node numbers in stat(1) */
    fuse_opt_add_arg(&args, "-ouse_ino");

    res = fuse_main(args.argc, args.argv, &motefs_ops, NULL);

    return res;
}