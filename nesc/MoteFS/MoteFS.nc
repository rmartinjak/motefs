#include "MoteFS.h"

interface MoteFS
{
    command void real_setNodes(struct motefs_node *nodes, uint8_t count);

    event void readBool(uint8_t node, const char *name, bool * val);
    event void readInt(uint8_t node, const char *name, int64_t *val);
    event void readStr(uint8_t node, const char *name, char val[MFS_DATA_SIZE]);
    command void readDone(error_t err);

    event void writeBool(uint8_t node, const char *name, bool val);
    event void writeInt(uint8_t node, const char *name, int64_t val);
    event void writeStr(uint8_t node, const char *name, char val[MFS_DATA_SIZE]);
    command void writeDone(error_t err);
}
