#include <stdio.h>
#include <time.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>

#include "storage.h"
#include "mailbox.h"
#include "log.h"
#include "framebuff.h"
#include "mqtt.h"


void *pthread_collect(void *arg)
{
    srand(time(NULL));
    Data_t tmpdata;

    sleep(2);

    while(1)
    {
        tmpdata.Tem = rand() % 10000 / 100.0;
        tmpdata.Hum = rand() % 10000 / 100.0;
        tmpdata.Elec = rand() % 10000 / 100.0;
        tmpdata.Volt = rand() % 10000 / 100.0;

        MailBoxSendMsg("storage", tmpdata);
        MailBoxSendMsg("mqtt", tmpdata);
        MailBoxSendMsg("framebuff", tmpdata);

        sleep(2);
    }

    return 0;
}

void *pthread_storage(void *arg)
{
    Data_t data = {0};

    StorageInit("info.db");

    while(1)
    {
        MailBoxRecvMsg(&data);

        StorageInsertData(&data);
    }

    StorageDeInit();

    return 0;
}

void *pthread_mqtt(void *arg)
{
    Data_t data = {0};

    mqtt_init();
    while(1)
    {
        MailBoxRecvMsg(&data);

        mqtt_send("Tem", data.Tem);
        mqtt_send("Hum", data.Hum);
        mqtt_send("Volt", data.Volt);
        mqtt_send("Elec", data.Elec);
    }
    mqtt_deinit();

    return 0;
}

void *pthread_framebuff(void *arg)
{
    Data_t data = {0};
    FB_T *pfb = NULL;
    char tmpbuff[256] = {0};

    pfb = FrameBuffInit("/dev/fb0");

    ClearScreen(pfb, 0, 0, 0);

    DrawBmp(pfb, 0, 0, "background.bmp");
 
    while(1)
    {
        MailBoxRecvMsg(&data);

        sprintf(tmpbuff, "Tem:  %.2lf", data.Tem);
        DrawString(pfb, 400, 200, tmpbuff, 255, 255, 255, 34, 177, 76);

        sprintf(tmpbuff, "Hum:  %.2lf", data.Hum);
        DrawString(pfb, 400, 300, tmpbuff, 255, 255, 255, 34, 177, 76);

        sprintf(tmpbuff, "Volt:  %.2lf", data.Volt);
        DrawString(pfb, 400, 400, tmpbuff, 255, 255, 255, 34, 177, 76);

        sprintf(tmpbuff, "Elec:  %.2lf", data.Elec);
        DrawString(pfb, 400, 500, tmpbuff, 255, 255, 255, 34, 177, 76);
    }
    
    FrameBuffDestroy(&pfb);

    return 0;
}


int main(void)
{
    char ch = 0;

    LogInit("log");
    SetCurLogLevel(LOG_LEVEL_INFO);

    MailBoxInit();

    RegisterMailBoxTask("collect", pthread_collect);
    RegisterMailBoxTask("storage", pthread_storage);
    RegisterMailBoxTask("mqtt", pthread_mqtt);
    RegisterMailBoxTask("framebuff", pthread_framebuff);

    while(1)
    {
        ch = getchar();
        getchar();

        if(ch == 'q')
        {
            break;
            exit(0);
        }
    }


    MailBoxWaitAllTask();
    LogDeInit();

    return 0;
}



// int main(void)
// {
// 	mqtt_init();
//     while(1)
//     {
//     mqtt_send("temp", 11.14);
//     mqtt_send("hum", 22.15);
//     mqtt_send("Co2", 33.16);
//     }
//     mqtt_deinit();
// 	return 0;
// }









// int main(void)
// {
//     FB_T *pfb = NULL;
//     double tmp = 0;
//     double hum = 0;
//     char tmpbuff[32] = {0};

//     srand(time(NULL));

//     pfb = FrameBuffInit("/dev/fb0");

//     ClearScreen(pfb, 0, 0, 0);

//     DrawBmp(pfb, 0, 0, "background.bmp");

//     while(20)
//     {
//         tmp = rand()%10000/100.0;
//         sprintf(tmpbuff, "%.2lf", tmp);
//         DrawString(pfb, 384, 369, tmpbuff, 255, 255, 255, 34, 177, 76);

//         hum = rand()%10000/100.0;
//         sprintf(tmpbuff, "%.2lf", hum);
//         DrawString(pfb, 384, 431, tmpbuff, 255, 255, 255, 34, 177, 76);

//         sleep(1);
//     }

//     FrameBuffDestroy(&pfb);

//     ClearScreen(pfb, 0, 0, 0);

//     DrawHorLine(pfb, 100, 100, 100, 255, 0, 0);

//     DrawVerLine(pfb, 200, 100, 100, 0, 255, 0);

//     DrawRect(pfb, 400, 300, 100, 200, 0, 0, 255);

//     DrawSolidRect(pfb, 200, 300, 50, 50, 0, 0, 255);


//     LogInit("log");
//     SetCurLogLevel(LOG_LEVEL_ERROR);

//     LogWrite(LOG_LEVEL_FATAL, "要爆炸了，快跑！");
//     LogWrite(LOG_LEVEL_ERROR, "hello world ");

//     LogDeInit();

//     return 0;
// }
