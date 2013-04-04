#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <inttypes.h>
#include <time.h>
#include <errno.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <fuse_opt.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>

#include "pack.h"
#include "serial.h"
#include "mfsmsg.h"
#include "mfsconst.h"

#define DEFAULT_RATE 11520
#define DEFAULT_PORT 9001

static char device[1024];
static unsigned rate = DEFAULT_RATE, port = DEFAULT_PORT;


#define MFS_MODE(x) ((x) & MFS_RDWR)
#define MFS_TYPE(x) ((x) & (MFS_BOOL|MFS_INT|MFS_STR))

static int node_count = 0;
static struct motefs_node
{
    char name[MFS_DATA_SIZE];
    uint8_t type;
} nodes[256];

static int fetch_nodecount(int *count)
{
    int op, result, res = 0;

    serial_lock();

    res = serial_send(0, MFS_OP_NODECOUNT, NULL, 0);
    if (res == -1)
        goto ret;

    res = serial_receive(NULL, &op, &result, NULL, 0);
    if (res == -1 || !result || op != MFS_OP_NODECOUNT)
    {
        res = -EIO;
        goto ret;
    }

    *count = result;
    res = 0;

  ret:
    serial_unlock();
    return res;
}

static int fetch_nodelist(struct motefs_node *nodes)
{
    int n, i, k, op, result, res = 0;
    uint8_t buf[MFS_DATA_SIZE];

    serial_lock();

    if (serial_send(0, MFS_OP_NODELIST, NULL, 0))
    {
        res = -EIO;
        goto ret;
    }

    /* the mote should send exactly `node_count` packets */
    for (i = 0; i < node_count; i++)
    {
        res = serial_receive(&n, &op, &result, buf, sizeof buf);
        if (res == -1 || !result || !(op & MFS_OP_NODELIST))
        {
            res = -1;
            goto ret;
        }

        nodes[n].type = result;
        for (k = 0; k < MFS_DATA_SIZE; k++)
            nodes[n].name[k] = buf[k];
    }

  ret:
    serial_unlock();
    if (res < 0)
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

    stbuf->st_nlink = 1;
    stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(NULL);
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();


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

    /* times aren't recorded, but current date and time looks better than
     * 1970-01-01 00:00 */
    stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(NULL);

    /* all files are regular files */
    stbuf->st_mode = S_IFREG;

    /* permission bits */
    if (nodes[n].type & MFS_RDONLY)
        stbuf->st_mode |= 0444;
    if (nodes[n].type & MFS_WRONLY)
        stbuf->st_mode |= 0200;

    /* size */
    switch (MFS_TYPE(nodes[n].type))
    {
        case MFS_BOOL:
            stbuf->st_size = 2;
            break;

        case MFS_INT:
            stbuf->st_size = snprintf(NULL, 0, "%" PRIi64 "\n", INT64_MIN);
            break;

        case MFS_STR:
            stbuf->st_size = MFS_DATA_SIZE + 1;
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

    /* the root is the only valid directory */
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
    (void) fi;

    int n;

    /* just check if `path` is valid */
    n = get_node(path);
    if (n == -1)
        return -ENOENT;

    return -0;
}

static int op_read(const char *path, char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi)
{
    (void) fi;
    (void) offset;

    int n, i, op, result, res = 0;
    uint8_t data[MFS_DATA_SIZE];

    n = get_node(path);

    if (n == -1)
        return -ENOENT;

    if (size <= MFSMSG_SIZE)
        return -EIO;

    serial_lock();

    if (serial_send(n, MFS_OP_READ, NULL, 0))
    {
        res = -EIO;
        goto ret;
    }

    res = serial_receive(NULL, &op, &result, data, sizeof data);
    if (res == -1 || !result || op != MFS_OP_READ)
    {
        res = -EIO;
        goto ret;
    }

    /* format reply */
    switch (MFS_TYPE(nodes[n].type))
    {
        case MFS_BOOL:
            strcpy(buf, (*data) ? "1" : "0");
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

    /* add a newline because that looks better */
    strcat(buf, "\n");
    res = strlen(buf);

  ret:
    serial_unlock();
    return res;
}

static int op_truncate(const char *path, off_t offset)
{
    (void) path;
    (void) offset;
    /* do nothing because we're not working with actual files */
    return 0;
}

static int op_write(const char *path, const char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi)
{
    (void) fi;
    (void) offset;

    int n, op, result, res = 0;
    uint8_t data[MFS_DATA_SIZE];

    size_t i, len;
    char line[MFS_DATA_SIZE];

    n = get_node(path);

    if (n == -1)
        return -ENOENT;

    /* copy one line without '\n' to temporary buffer
     * (and truncate it if too long) */
    for (i = 0; i < MFS_DATA_SIZE - 1 && i < size && buf[i] != '\n'; i++)
        line[i] = buf[i];
    line[i] = '\0';
    len = i;


    /* convert the line to appropriate format which can be send as a serial
     * packet */
    if (nodes[n].type & MFS_BOOL)
    {
        if (!strcmp(line, "true"))
            data[0] = 1;
        else if (!strcmp(line, "false"))
            data[0] = 0;
        else
            data[0] = atoi(line) != 0;
        len = 1;
    }
    else if (nodes[n].type & MFS_INT)
    {
        int64_t val = atoll(line);
        len = pack(data, "l", val);
    }
    else if (nodes[n].type & MFS_STR)
    {
        unsigned i;
        for (i = 0; i < len; i++)
            data[i] = line[i];
        data[len] = '\0';
    }

    serial_lock();

    if (serial_send(n, MFS_OP_WRITE, data, len))
    {
        res = -EIO;
        goto ret;
    }

    res = serial_receive(NULL, &op, &result, NULL, 0);
    if (res == -1 || !result || op != MFS_OP_WRITE)
    {
        res = -EIO;
        goto ret;
    }

  ret:
    serial_unlock();
    if (!res)
        return size;
    return res;
}


static struct fuse_operations motefs_ops = {
    .getattr = op_getattr,
    .readdir = op_readdir,
    .open = op_open,
    .read = op_read,
    .truncate = op_truncate,
    .write = op_write,
};


static void set_device(const char *arg)
{
    const char *p;
    size_t n = sizeof device - 1;

    p = strchr(arg, ':');
    if (p)
    {
        n = p++ - arg;
        rate = port = atoi(p);
    }
    strncpy(device, arg, n);
}

static int motefs_opt_proc(void *data, const char *arg, int key,
                           struct fuse_args *outargs)
{
    (void) data;
    (void) outargs;

    switch (key)
    {
        case FUSE_OPT_KEY_NONOPT:
            /* serial device (or sf host) is already set, pass the arg on to fuse */
            if (device[0])
                return 1;
            set_device(arg);
            return 0;
    }
    /* key unknown */
    return 1;
}


int main(int argc, char **argv)
{
    int res;
    struct stat st;
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

    if (fuse_opt_parse(&args, NULL, NULL, motefs_opt_proc) == -1)
        return EXIT_FAILURE;

    if (!device[0])
    {
        fprintf(stderr, "usage: %s path_or_host[:rate_or_port] mount_point\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    /* if `device` is not a file, treat it like the hostname of a serial
     * forwarder */
    if (lstat(device, &st) == -1 && errno == ENOENT)
        res = serial_connect_sf(device, port);
    else
        res = serial_connect_dev(device, rate);

    if (res)
        return EXIT_FAILURE;

    if (fetch_nodecount(&node_count) || fetch_nodelist(nodes))
    {
        fprintf(stderr, "can't get node list from Mote\n");
        return EXIT_FAILURE;
    }

    /* add "use_ino" to display node numbers in stat(1) */
    fuse_opt_add_arg(&args, "-ouse_ino");

    /* let fuse do permission checking */
    fuse_opt_add_arg(&args, "-odefault_permissions");

    res = fuse_main(args.argc, args.argv, &motefs_ops, NULL);

    return res;
}
