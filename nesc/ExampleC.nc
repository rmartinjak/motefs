#include "MoteFS.h"
#include <string.h>

module ExampleC
{
    uses
    {
        interface Boot;
        interface Leds;
        interface MoteFS;
        interface SplitControl as AMControl;
    }
}

implementation
{

    char teststr[MFS_DATA_SIZE] = "Hello, world!";

    /* define the file system nodes */
    struct motefs_node nodes[] = {
        {"led0", MFS_BOOL | MFS_RDWR},
        {"led1", MFS_BOOL | MFS_RDWR},
        {"led2", MFS_BOOL | MFS_RDWR},
        {"leds", MFS_INT | MFS_RDWR},
        {"test", MFS_STR | MFS_RDWR},
        {"TOS_NODE_ID", MFS_INT | MFS_RDONLY},
    };


    event void Boot.booted(void)
    {
        call Leds.led0On();
        call AMControl.start();

        /* let MoteFS know about the file system nodes */
        call MoteFS.setNodes(nodes);
    }

    event void AMControl.startDone(error_t err)
    {
        if (err == SUCCESS)
        {
            call Leds.led0Off();
        }
        else
        {
            call AMControl.start();
        }
    }

    event void AMControl.stopDone(error_t err)
    {
    }

    event void MoteFS.readBool(uint8_t node, const char *name, bool * val)
    {
        uint8_t led = 0, current = call Leds.get();
        error_t res = SUCCESS;

        if (!strcmp(name, "led0"))
        {
            led = LEDS_LED0;
        }
        else if (!strcmp(name, "led1"))
        {
            led = LEDS_LED1;
        }
        else if (!strcmp(name, "led2"))
        {
            led = LEDS_LED2;
        }
        else
        {
            res = FAIL;
        }

        *val = (current & led);

        call MoteFS.readDone(res);
    }

    event void MoteFS.readInt(uint8_t node, const char *name, int64_t *val)
    {
        error_t res = SUCCESS;

        if (!strcmp(name, "leds"))
        {
            *val = call Leds.get();
        }
        else if (!strcmp(name, "TOS_NODE_ID"))
        {
            *val = TOS_NODE_ID;
        }
        else
        {
            res = FAIL;
        }

        call MoteFS.readDone(res);
    }

    event void MoteFS.readString(uint8_t node, const char *name, char val[MFS_DATA_SIZE])
    {
        error_t res = SUCCESS;

        if (!strcmp(name, "test"))
        {
            memcpy(val, teststr, MFS_DATA_SIZE);
        }
        else
        {
            res = FAIL;
        }

        call MoteFS.readDone(res);
    }


    event void MoteFS.writeBool(uint8_t node, const char *name, bool val)
    {
        uint8_t led = 0, current = call Leds.get();
        error_t res = SUCCESS;

        if (!strcmp(name, "led0"))
        {
            led = LEDS_LED0;
        }
        else if (!strcmp(name, "led1"))
        {
            led = LEDS_LED1;
        }
        else if (!strcmp(name, "led2"))
        {
            led = LEDS_LED2;
        }
        else
        {
            res = FAIL;
        }

        if (val)
        {
            current |= led;
        }
        else
        {
            current &= ~led;
        }

        call Leds.set(current);

        call MoteFS.writeDone(res);
    }

    event void MoteFS.writeInt(uint8_t node, const char *name, int64_t val)
    {
        error_t res = SUCCESS;

        if (!strcmp(name, "leds"))
        {
            call Leds.set(val);
        }
        else
        {
            res = FAIL;
        }

        call MoteFS.writeDone(res);
    }

    event void MoteFS.writeString(uint8_t node, const char *name, char val[MFS_DATA_SIZE])
    {
        error_t res = SUCCESS;

        if (!strcmp(name, "test"))
        {
            memcpy(teststr, val, MFS_DATA_SIZE);
        }
        else
        {
            res = FAIL;
        }

        call MoteFS.writeDone(res);
    }
}
