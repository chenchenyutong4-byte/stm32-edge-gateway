#ifndef PAL_H
#define PAL_H

#include "main.h"
#include <string.h>
#include "usart.h"
#include "stm32f4xx.h"
#include "stdio.h"

#define WIFI_SSID   "YOUR_WIFI_SSID"
#define WIFI_PASS   "YOUR_WIFI_PASSWORD"
#define MQTT_BROKER "mqtt.example.com"
#define MQTT_PORT   1883
#define MQTT_UID    "00000000000000000000000000000000"
#define TOPIC_PUB    "stm32"

typedef struct {
    uint8_t client_id[50];
    uint8_t username[24];
    uint8_t password[24];
    int socket;
    uint32_t keepalive_interval;
    uint8_t clean_session;

    int (*send)(void* sock, const void *buf, unsigned int len);
    int (*recv)(int sock, uint8_t *buf, int buf_len, int timeout);

    void *socket_info;
} mqtt_broker_handle_t;

int pal_tcp_recv_raw(int sock, uint8_t *buf, int len, int timeout_ms);

void MQTT_SendTempHumi_NoWait(float temp, float humi,float light);

void MQTT_Init(void);
void RE_MQTT_Init();

int MQTT_SendTempHumi(float temp, float humi);
void MQTT_SendPing(void);

void MQTT_ReConnect();
#endif
