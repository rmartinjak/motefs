#include "MoteFS.h"

interface MoteFS
{
    command void real_setNodes(struct motefs_node *nodes, uint8_t count);

    event void readBool(const char *name, bool * val);
    event void readInt(const char *name, int64_t *val);
    event void readStr(const char *name, char val[MFS_DATA_SIZE]);
    command void readDone(error_t err);

    event void writeBool(const char *name, bool val);
    event void writeInt(const char *name, int64_t val);
    event void writeStr(const char *name, char val[MFS_DATA_SIZE]);
    command void writeDone(error_t err);
}
