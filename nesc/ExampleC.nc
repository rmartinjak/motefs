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

    char teststr[MFS_DATA_SIZE];

    struct motefs_node nodes[] = {
        {"led0", MFS_BOOL, MFS_READWRITE},
        {"led1", MFS_BOOL, MFS_READWRITE},
        {"led2", MFS_BOOL, MFS_READWRITE},
        {"leds", MFS_INT, MFS_READWRITE},
        {"test", MFS_STR, MFS_READWRITE},
    };


    event void Boot.booted(void)
    {
        call Leds.led0On();
        call AMControl.start();
        call MoteFS.init(nodes, sizeof nodes / sizeof nodes[0]);
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

    event void MoteFS.readBool(const char *name, bool * val)
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

    event void MoteFS.readInt(const char *name, int64_t *val)
    {
        error_t res = SUCCESS;

        if (!strcmp(name, "leds"))
        {
            *val = call Leds.get();
        }
        else
        {
            res = FAIL;
        }

        call MoteFS.readDone(res);
    }

    event void MoteFS.readStr(const char *name, char val[MFS_DATA_SIZE])
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


    event void MoteFS.writeBool(const char *name, bool val)
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

    event void MoteFS.writeInt(const char *name, int64_t val)
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

    event void MoteFS.writeStr(const char *name, char val[MFS_DATA_SIZE])
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
