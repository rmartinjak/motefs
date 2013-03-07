#include <AM.h>
#include "MoteFS.h"

configuration ExampleAppC
{
}

implementation
{
    components ExampleC as App;

    components MainC, LedsC;
    App.Boot -> MainC;
    App.Leds -> LedsC;

    components new MoteFSC(AM_BROADCAST_ADDR);
    App.MoteFS -> MoteFSC;

    components SerialActiveMessageC;
    App.AMControl -> SerialActiveMessageC;

    components new SerialAMSenderC(AM_MOTEFS_MSG) as AMSend;
    components new SerialAMReceiverC(AM_MOTEFS_MSG) as AMReceive;
    MoteFSC.AMSend -> AMSend;
    MoteFSC.Receive -> AMReceive;
}
