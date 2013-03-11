#include <AM.h>
#include <string.h>
#include "MoteFS.h"
#include "pack.h"

static void strtonx(nx_uint8_t *dest, const char *src)
{
    int i;
    for (i = 0; i < MFS_DATA_SIZE; i++)
        dest[i] = src[i];
}

static void nxtostr(char *dest, const nx_uint8_t *src)
{
    int i;
    for (i = 0; i < MFS_DATA_SIZE; i++)
        dest[i] = src[i];
}


generic module MoteFSP(am_addr_t msgdest)
{
    provides interface MoteFS;

    uses
    {
        interface AMSend as Send;
        interface Receive;
        interface Queue<nx_struct motefs_msg> as MsgQueue;
        interface Leds;
    }
}

implementation
{
    void enqueueMsg(void);
    task void sendMsg(void);


    struct motefs_node *mfs_nodes = NULL;
    uint8_t mfs_nodecount = 0;

    message_t pkt;
    nx_struct motefs_msg reply;

    uint8_t current_node, current_type;

    bool buf_bool;
    int64_t buf_int;
    char buf_str[MFS_DATA_SIZE];


    void enqueueMsg(void)
    {
        bool q_empty = call MsgQueue.empty();
        call MsgQueue.enqueue(reply);

        if (q_empty)
        {
            post sendMsg();
        }
    }

    task void sendMsg(void)
    {
        nx_struct motefs_msg *outmsg;

        if (call MsgQueue.empty())
        {
            return;
        }

        outmsg = call Send.getPayload(&pkt, sizeof *outmsg);
        if (!outmsg)
        {
            return;
        }

        /* copy message */
        *outmsg = call MsgQueue.dequeue();

        if (call Send.send(msgdest, &pkt, sizeof *outmsg) == SUCCESS)
        {
        }
        else
        {
            call MsgQueue.enqueue(*outmsg);
        }
    }

    event void Send.sendDone(message_t *msg, error_t error)
    {
        if (error != SUCCESS)
        {
            nx_struct motefs_msg *payload =
                call Send.getPayload(msg, sizeof *payload);
            call MsgQueue.enqueue(*payload);
        }

        call Leds.led1Toggle();
        post sendMsg();
    }


    command void MoteFS.real_setNodes(struct motefs_node *nodes,
                                      uint8_t count)
    {
        mfs_nodes = nodes;
        mfs_nodecount = count;
    }


    command void MoteFS.readDone(error_t error)
    {
        memset(reply.data, 0, sizeof reply.data);
        switch (current_type)
        {
            case MFS_BOOL:
                pack(reply.data, "b", (uint8_t) buf_bool);
                break;

            case MFS_INT:
                pack(reply.data, "l", buf_int);
                break;

            case MFS_STR:
                strtonx(reply.data, buf_str);
                break;
        }

        reply.node = current_node;
        reply.op = MFS_OP_READ;
        reply.result = (error == SUCCESS);

        enqueueMsg();
    }

    command void MoteFS.writeDone(error_t error)
    {
        memset(reply.data, 0, sizeof reply.data);
        reply.node = current_node;
        reply.op = MFS_OP_WRITE;
        reply.result = (error == SUCCESS);
        enqueueMsg();
    }


    task void listNodes(void)
    {
        uint8_t i = 0;
        for (i = 0; i < mfs_nodecount; i++)
        {
            memset(reply.data, 0, sizeof reply.data);
            strtonx(reply.data, mfs_nodes[i].name);
            reply.node = i;
            reply.op = MFS_OP_NODELIST;
            reply.result = mfs_nodes[i].type;
            enqueueMsg();
        }
    }


    event message_t *Receive.receive(message_t *msg, void *payload,
                                     uint8_t len)
    {
        nx_struct motefs_msg *m;
        struct motefs_node *node;

        if (len != sizeof *m)
        {
            return msg;
        }
        call Leds.led2Toggle();

        m = payload;
        if (m->op == MFS_OP_NODELIST)
        {
            post listNodes();
            return msg;
        }
        else if (m->op == MFS_OP_NODECOUNT)
        {
            memset(reply.data, 0, sizeof reply.data);
            reply.node = -1;
            reply.op = MFS_OP_NODECOUNT;
            reply.result = mfs_nodecount;
            enqueueMsg();
            return msg;
        }

        node = &mfs_nodes[m->node];

        current_node = m->node;
        current_type = MFS_TYPE(node->type);

        switch (current_type)
        {
            case MFS_BOOL:
                if (m->op == MFS_OP_READ)
                {
                    signal MoteFS.readBool(m->node, node->name, &buf_bool);
                }
                else
                {
                    uint8_t val;
                    unpack(m->data, "b", &val);
                    signal MoteFS.writeBool(m->node, node->name, val);
                }
                break;

            case MFS_INT:
                if (m->op == MFS_OP_READ)
                {
                    signal MoteFS.readInt(m->node, node->name, &buf_int);
                }
                else
                {
                    int64_t val;
                    unpack(m->data, "l", &val);
                    signal MoteFS.writeInt(m->node, node->name, val);
                }
                break;

            case MFS_STR:
                if (m->op == MFS_OP_READ)
                {
                    signal MoteFS.readStr(m->node, node->name, buf_str);
                }
                else
                {
                    nxtostr(buf_str, m->data);
                    signal MoteFS.writeStr(m->node, node->name, buf_str);
                }
        }
        return msg;
    }
}
