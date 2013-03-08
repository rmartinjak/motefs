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
#ifdef LEDS
        interface Leds;
#endif
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

    enum motefs_type current_type;
    char current_name[MFS_DATA_SIZE];

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
#ifdef LEDS
            call Leds.led1Toggle();
#endif
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

        post sendMsg();
    }


    command void MoteFS.init(struct motefs_node *nodes, uint8_t count)
    {
        mfs_nodes = nodes;
        mfs_nodecount = count;
    }


    command void MoteFS.readDone(error_t error)
    {
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

        reply.op = MFS_READ;
        reply.result = (error == SUCCESS);

        enqueueMsg();
    }

    command void MoteFS.writeDone(error_t error)
    {
        reply.op = MFS_WRITE;
        reply.result = (error == SUCCESS);
        enqueueMsg();
    }


    task void listNodes(void)
    {
        uint8_t i = 0;

        reply.op = MFS_LIST;

#ifdef LEDS
        call Leds.led0Toggle();
#endif
        for (i = 0; mfs_nodecount; i++)
        {
            strtonx(reply.data, mfs_nodes[i].name);
            reply.node = i;
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

        m = payload;
        if (m->op == MFS_LIST)
        {
            post listNodes();
            return msg;
        }
        else if (m->op == MFS_NODECOUNT)
        {
            reply.op = MFS_NODECOUNT;
            reply.result = mfs_nodecount;
            enqueueMsg();
            return msg;
        }
#ifdef LEDS
        call Leds.led2Toggle();
#endif

        node = &mfs_nodes[m->node];

        current_type = node->type;
        strcpy(current_name, node->name);

        switch (node->type)
        {
            case MFS_BOOL:
                if (m->op == MFS_READ)
                {
                    signal MoteFS.readBool(current_name, &buf_bool);
                }
                else
                {
                    nx_uint8_t val;
                    val = unpack(m->data, "b", &val);
                    signal MoteFS.writeBool(current_name, val);
                }
                break;

            case MFS_INT:
                if (m->op == MFS_READ)
                {
                    signal MoteFS.readInt(current_name, &buf_int);
                }
                else
                {
                    nx_int64_t val;
                    val = unpack(m->data, "l", &val);
                    signal MoteFS.writeInt(current_name, val);
                }
                break;

            case MFS_STR:
                if (m->op == MFS_READ)
                {
                    signal MoteFS.readStr(current_name, buf_str);
                }
                else
                {
                    nxtostr(buf_str, m->data);
                    signal MoteFS.writeStr(current_name, buf_str);
                }
        }
        return msg;
    }
}
