// vim: ft=nc

#include <message.h>

#ifndef MFS_H
#define MFS_H

// "mfs header" is 3 bytes (see below)
#if (MFS_DATA_SIZE + 3 > TOSH_DATA_LENGTH)
#error MFS_DATA_SIZE too big
#endif

enum motefs_type
{
    MFS_BOOL,
    MFS_INT,
    MFS_STR,
};

enum motefs_op
{
    MFS_READ,
    MFS_WRITE,
    MFS_READWRITE,
    MFS_NODECOUNT,
    MFS_LIST,
};

struct motefs_node
{
    char name[MFS_DATA_SIZE];
    enum motefs_type type;
    enum motefs_op op;
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
