#include <AM.h>
generic configuration MoteFSC(am_addr_t msgdest)
{
    provides interface MoteFS;

    uses
    {
        interface AMSend;
        interface Receive;
    }
}

implementation
{
    components new MoteFSP(msgdest);

    MoteFS = MoteFSP;

    components new QueueC(nx_struct motefs_msg, 20);
    MoteFSP.MsgQueue->QueueC;

    MoteFSP.Send = AMSend;
    MoteFSP.Receive = Receive;
#ifdef LEDS
    components LedsC;
    MoteFSP.Leds -> LedsC;
#endif
}
