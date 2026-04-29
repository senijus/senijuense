//https://eclipse.dev/paho/files/mqttdoc/MQTTClient/html/_m_q_t_t_client_8h.html#a9a0518d9ca924d12c1329dbe3de5f2b6
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include "mqtt.h"
#include "log.h"  // 引入自定义日志模块

static char topic[2][200] = {0};
static MQTTClient client;
static int id = 10000;
volatile static MQTTClient_deliveryToken deliveredtoken;

// 组装MQTT订阅/发布主题
void pack_topic(char * dev_name, char * pro_id)
{
    // 订阅主题：接收属性上报回复
    sprintf(topic[0], "$sys/%s/%s/thing/property/post/reply", pro_id, dev_name);
    // 发布主题：上报设备属性
    sprintf(topic[1], "$sys/%s/%s/thing/property/post", pro_id, dev_name);
}

// 消息发送成功回调
void delivered(void *context, MQTTClient_deliveryToken dt)
{
    char log_buff[128] = {0};
    sprintf(log_buff, "Message with token value %d delivery confirmed", dt);
    LogWrite(LOG_LEVEL_INFO, log_buff);  // 记录消息投递成功日志
    deliveredtoken = dt;
}

// 接收消息回调
int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    int i;
    char* payloadptr;
    char log_buff[256] = {0};

    // 记录消息到达日志
    sprintf(log_buff, "Message arrived, topic: %s", topicName);
    LogWrite(LOG_LEVEL_INFO, log_buff);

    // 记录消息内容
    LogWrite(LOG_LEVEL_INFO, "message content: ");
    payloadptr = (char*)message->payload;
    for(i=0; i<message->payloadlen; i++)
    {
        log_buff[i] = *payloadptr++;
    }
    LogWrite(LOG_LEVEL_INFO, log_buff);

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

// 连接断开回调
void connlost(void *context, char *cause)
{
    char log_buff[128] = {0};
    sprintf(log_buff, "Connection lost, cause: %s", cause);
    LogWrite(LOG_LEVEL_ERROR, log_buff);  // 记录连接断开错误日志
}

// MQTT客户端初始化
int mqtt_init()
{
    // 组装订阅/发布主题
    pack_topic(DEV_NAME, PRODUCT_ID);
    
    // 创建MQTT客户端实例
    int rc = MQTTClient_create(&client, NEW_ADDRESS, CLIENTID,
                               MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if(MQTTCLIENT_SUCCESS != rc)
    {
        LogWrite(LOG_LEVEL_ERROR, "create mqtt client failure");  // 记录客户端创建失败日志
        exit(1);
    }
    
    // 设置连接参数
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    conn_opts.keepAliveInterval = 20;  // 心跳间隔20秒
    conn_opts.cleansession = 1;        // 清理会话
    conn_opts.username = PRODUCT_ID;
    conn_opts.password = PASSWD;

    // 设置回调函数
    rc = MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, delivered);
    if(MQTTCLIENT_SUCCESS != rc )
    {
        char log_buff[128] = {0};
        sprintf(log_buff, "Failed to set callbacks, return code %d", rc);
        LogWrite(LOG_LEVEL_ERROR, log_buff);  // 记录回调设置失败日志
        exit(EXIT_FAILURE);
    }

    // 连接MQTT服务器
    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        char log_buff[128] = {0};
        sprintf(log_buff, "Failed to connect to mqtt server, return code %d", rc);
        LogWrite(LOG_LEVEL_ERROR, log_buff);  // 记录连接服务器失败日志
        exit(EXIT_FAILURE);
    }

#if 0
    //订阅单个主题  如果需要订阅的话    
    MQTTClient_subscribe(client, topic[0], QOS);
    char sub_log[128] = {0};
    sprintf(sub_log, "Subscribing to topic %s for client %s using QoS%d", topic[0], CLIENTID, QOS);
    LogWrite(LOG_LEVEL_INFO, sub_log);
#endif 

    LogWrite(LOG_LEVEL_INFO, "mqtt client init success");  // 记录初始化成功日志
	return rc;
}

// 发送MQTT消息
int mqtt_send(char * key, double value)
{
    MQTTClient_deliveryToken deliveryToken;
    MQTTClient_message test2_pubmsg = MQTTClient_message_initializer;
    char message[1024] = {0};  // 消息缓冲区
    test2_pubmsg.qos = QOS;
    test2_pubmsg.retained = 0;
    test2_pubmsg.payload = message;

    // 组装JSON格式消息体
    sprintf(message,"{\"id\":\"%d\",\"version\":\"1.0\",\"params\":{\"%s\":{\"value\":%.2lf}}}",id++, key, value);
    test2_pubmsg.payloadlen = strlen(message);

    // 记录待发送的消息内容
    LogWrite(LOG_LEVEL_DEBUG, message);

    // 发布消息
    int rc = MQTTClient_publishMessage(client, topic[1], &test2_pubmsg, &deliveryToken);
    if(MQTTCLIENT_SUCCESS != rc)
    {
        char log_buff[128] = {0};
        sprintf(log_buff, "mqtt publish failure, thread id: %lu", pthread_self());
        LogWrite(LOG_LEVEL_ERROR, log_buff);  // 记录发布失败日志
        exit(1);
    }

    // 记录发布成功日志
    char log_buff[256] = {0};
    sprintf(log_buff, "Waiting for publication confirm, topic: %s, client: %s, timeout: %d秒", 
            topic[1], CLIENTID, (int)(TIMEOUT/1000));
    LogWrite(LOG_LEVEL_INFO, log_buff);

    // 等待消息发布完成确认
    MQTTClient_waitForCompletion(client, deliveryToken, TIMEOUT);
    sleep(1);

    return rc;
}

// 释放MQTT客户端资源
void mqtt_deinit()
{
    // 断开连接（超时10秒）
    MQTTClient_disconnect(client, 10000);
    // 销毁客户端实例
    MQTTClient_destroy(&client);
    LogWrite(LOG_LEVEL_INFO, "mqtt client deinit success");  // 记录反初始化成功日志
}

