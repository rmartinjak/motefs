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
#include <pthread.h>

#include "pack.h"
#include "serial.h"
#include "mfsmsg.h"
#include "mfsconst.h"


static int node_count = 0;
static struct motefs_node
{
    char name[MFS_DATA_SIZE];
    uint8_t type;
    uint8_t op;
} nodes[256];
#define MFS_ISREAD(node) ((node).type == MFS_READ || (node).type == MFS_READWRITE)
#define MFS_ISWRITE(node) ((node).type == MFS_WRITE || (node).type == MFS_READWRITE)


static char device[1024];
static unsigned baudrate;


static int fetch_nodecount(int *count)
{
    int op, result, res;

    serial_lock();

    res = serial_send(0, MFS_NODECOUNT, NULL, 0);
    if (res == -1)
        goto ret;

    res = serial_receive(NULL, &op, &result, NULL, 0);
    if (res == -1 || op != MFS_NODECOUNT)
        goto ret;

    *count = result;

  ret:
    serial_unlock();
    if (res)
        return -1;
    return 0;
}

static int fetch_nodelist(struct motefs_node *nodes)
{
    int n, i, k, op, result, res;
    uint8_t buf[MFS_DATA_SIZE];

    serial_lock();

    res = serial_send(0, MFS_LIST, NULL, 0);
    for (i = 0; i < node_count; i++)
    {
        res = serial_receive(&n, &op, &result, buf, sizeof buf);
        if (res == -1 || op != MFS_LIST)
            goto ret;

        nodes[n].op = op;
        nodes[n].type = result;
        for (k = 0; k < MFS_DATA_SIZE; k++)
            nodes[n].name[k] = buf[k];
    }

  ret:
    serial_unlock();
    if (res)
        return -1;
    return 0;
}

static int get_node(const char *name)
{
    int i;
    if (*name == '/')
        name++;

    if (!*name)
        return -1;

    for (i = 0; i < node_count; i++)
        if (!strcmp(name, nodes[i].name))
            return i;
    return -1;
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

    int i;

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

    int n, i, op, len, res;
    uint8_t data[MFS_DATA_SIZE];

    n = get_node(path);

    if (n == -1)
        return -ENOENT;

    if (size < MFSMSG_SIZE)
        return -EIO;

    serial_lock();

    if (serial_send(n, MFS_READ, NULL, 0))
    {
        res = -EIO;
        goto ret;
    }

    res = serial_receive(NULL, &op, &len, data, sizeof data);
    if (res == -1 || op != MFS_READ)
    {
        res = -EIO;
        goto ret;
    }

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
            for (i = 0; i < MFS_DATA_SIZE - 1; i++)
                buf[i] = data[i];
            buf[i] = '\0';
            break;
    }

    res = len;

  ret:
    serial_unlock();
    return res;
}

static int op_write(const char *path, const char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi)
{
    (void) fi;
    (void) offset;

    int n, op, result, res = 0;
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
    res = size;

    serial_lock();
    if (serial_send(n, MFS_WRITE, data, size))
    {
        res = -EIO;
        goto ret;
    }

    res = serial_receive(NULL, &op, &result, NULL, 0);
    if (res == -1 || op != MFS_WRITE)
    {
        res = -EIO;
        goto ret;
    }

  ret:
    serial_unlock();
    return res;
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
    static bool device_is_set = false;

    switch (key)
    {
        case FUSE_OPT_KEY_NONOPT:
            /* serial device is already set, pass the arg on to fuse */
            if (device_is_set)
                return 1;
            strncpy(device, arg, sizeof device - 1);
            device_is_set = true;
            return 0;

        case MOTEFS_OPT_BAUDRATE:
            if (!strchr(arg, '=') || !(baudrate = atoi(strchr(arg, '=') + 1)))
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

    if (serial_connect(device, baudrate))
    {
        fprintf(stderr, "can't open device %s:%u\n", device, baudrate);
        return EXIT_FAILURE;
    }

    if (fetch_nodecount(&node_count) || fetch_nodelist(nodes))
    {
        fprintf(stderr, "can't get node list from Mote\n");
        return EXIT_FAILURE;
    }

    /* add "use_ino" to display node numbers in stat(1) */
    fuse_opt_add_arg(&args, "-ouse_ino");

    res = fuse_main(args.argc, args.argv, &motefs_ops, NULL);

    return res;
}
