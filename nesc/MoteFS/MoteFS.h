// vim: ft=nc

#include <message.h>

#ifndef MFS_H
#define MFS_H

#define setNodes(n) real_setNodes((n), sizeof (n) / sizeof (n)[0])

// "mfs header" is 3 bytes (see below)
#if (MFS_DATA_SIZE + 3 > TOSH_DATA_LENGTH)
#error MFS_DATA_SIZE too big
#endif

enum motefs_type
{
    MFS_RDONLY = (1 << 0),
    MFS_WRONLY = (1 << 1),
    MFS_RDWR = (MFS_RDONLY | MFS_WRONLY),
    MFS_BOOL = (1 << 2),
    MFS_INT = (1 << 3),
    MFS_STR = (1 << 4),
};
#define MFS_ISREAD(x) (!!((x) & MFS_RDONLY))
#define MFS_ISWRITE(x) (!!((x) & MFS_RDONLY))
#define MFS_ISRW(x) (((x) & MFS_RDWR) == MFS_RDWR)

#define MFS_MODE(x) ((x) & MFS_RDWR)
#define MFS_TYPE(x) ((x) & (MFS_BOOL|MFS_INT|MFS_STR))

struct motefs_node
{
    char name[MFS_DATA_SIZE];
    enum motefs_type type;
};

enum motefs_op
{
    MFS_OP_READ,
    MFS_OP_WRITE,
    MFS_OP_NODECOUNT,
    MFS_OP_NODELIST,
};


nx_struct motefs_msg
{
    nx_uint8_t node;
    nx_uint8_t op;
    nx_uint8_t result;
    nx_uint8_t data[MFS_DATA_SIZE];
};

enum
{ AM_MOTEFS_MSG = 42 };

#endif
