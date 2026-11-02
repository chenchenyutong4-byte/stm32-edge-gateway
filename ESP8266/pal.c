#include "esp_at.h"
#include "command.h"
#include "usart.h"
#include <string.h>
#include <stdio.h>
#include "stdlib.h"
#include "libemqtt.h"
#include "pal.h"
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "flash.h"
#include "Taskhandel.h"

extern uint8_t a[256];

void MQTT_Init(void)
{

	  HAL_UART_Transmit(&huart1, (uint8_t *)"+++\r\n", 3, 100);
    vTaskDelay(1000);

    ESP_SendCmd_OK("AT\r\n", 2000);
    vTaskDelay(200);

    ESP_SendCmd_OK("AT+CWMODE=1\r\n", 2000);
    ESP_SendCmd("AT+CWJAP=\"YOUR_WIFI_SSID\",\"YOUR_WIFI_PASSWORD\"\r\n", "WIFI GOT IP", 5000);
		vTaskDelay(500);

    ESP_SendCmd("AT+CIPSTART=\"TCP\",\"mqtt.example.com\",1883\r\n", "CONNECT", 2000);
		vTaskDelay(500);

    ESP_SendCmd_OK("AT+CIPMODE=1\r\n", 2000);

		ESP_SendCmd("AT+CIPSEND\r\n", ">", 5000);
    vTaskDelay(500);
    RingBuf_Clear();

uint8_t connect_packet[] = {
        0x10, 0x2E,

        0x00,0x06,
        0x4D,0x51,0x49,0x73,0x64,0x70,
        0x03, 0x02, 0x00,0x78,

        0x00,0x20,

        '0','0','0','0','0','0','0','0',
        '0','0','0','0','0','0','0','0',
        '0','0','0','0','0','0','0','0',
        '0','0','0','0','0','0','0','0'
};

				HAL_UART_Transmit(&huart1, connect_packet, sizeof(connect_packet), 5000);
				vTaskDelay(300);

    uint8_t connack[4];
		int ret = pal_tcp_recv_raw(0, connack, 4, 3000);
    if (ret == 4 && connack[0] == 0x20 && connack[1] == 0x02 && connack[3] == 0x00)
    {

		uint8_t sub_packet[] = {
				0x82, 0x0A,

				0x00, 0x01,

				0x00, 0x05,
				's','t','m','3','2',
				0x01
		};
		HAL_UART_Transmit(&huart1, sub_packet, sizeof(sub_packet), 5000);
		vTaskDelay(300);
		g_mqtt_connected = 1;
		Log_Write(LOG_INFO, "CONNECT FINISH");

    }
    else
    {
        g_mqtt_connected = 0;
    }
}

void RE_MQTT_Init(void)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)"+++\r\n", 3, 100);
    vTaskDelay(1000);
	for(int i=0;i<2;i++)
	{
		ESP_SendCmd_OK("AT+RST\r\n", 2000);
    vTaskDelay(pdMS_TO_TICKS(1000));
	}

    ESP_SendCmd_OK("AT+CWMODE=1\r\n", 2000);
    ESP_SendCmd("AT+CWJAP=\"YOUR_WIFI_SSID\",\"YOUR_WIFI_PASSWORD\"\r\n", "WIFI GOT IP", 5000);

    ESP_SendCmd("AT+CIPSTART=\"TCP\",\"mqtt.example.com\",1883\r\n", "CONNECT", 5000);

		ESP_SendCmd_OK("AT+CIPMODE=1\r\n", 2000);
		ESP_SendCmd("AT+CIPSEND\r\n", ">", 5000);
    vTaskDelay(500);
    RingBuf_Clear();

uint8_t connect_packet[] = {
        0x10, 0x2E,

        0x00,0x06,
        0x4D,0x51,0x49,0x73,0x64,0x70,
        0x03, 0x02, 0x00,0x78,

        0x00,0x20,

        '0','0','0','0','0','0','0','0',
        '0','0','0','0','0','0','0','0',
        '0','0','0','0','0','0','0','0',
        '0','0','0','0','0','0','0','0'
};

				HAL_UART_Transmit(&huart1, connect_packet, sizeof(connect_packet), 5000);
				vTaskDelay(300);

    uint8_t connack[4];
    int ret = pal_tcp_recv_raw(0, connack, 4, 3000);

    if (ret <1)
    {
			RingBuf_Clear();
			for(int i=0;i<30;i++)
			{
				  HAL_UART_Transmit(&huart1, connect_packet, sizeof(connect_packet), 5000);
					vTaskDelay(200);
			}

		uint8_t sub_packet[] = {
				0x82, 0x0A,

				0x00, 0x01,

				0x00, 0x05,
				's','t','m','3','2',
				0x01
		};
		HAL_UART_Transmit(&huart1, sub_packet, sizeof(sub_packet), 5000);
		vTaskDelay(300);

				g_mqtt_connected = 1;
				connect=1;
				Log_Write(LOG_INFO, "reconnect success");
				return ;
			}

}

// ==================== MQTT 心跳包 ====================
void MQTT_SendPing(void)
{
  uint8_t ping[] = {0xC0, 0x00};
  HAL_UART_Transmit(&huart1, ping, 2, 2000);
}

/**
 * @brief  透传模式下从 ESP8266 接收原始 TCP 数据
 * @param  sock       未使用（保留参数，兼容之前的接口设计）
 * @param  buf        接收缓冲区指针
 * @param  len        期望接收的字节数
 * @param  timeout_ms 超时时间（毫秒）
 * @return 实际接收到的字节数，超时或失败返回 -1
 */

int pal_tcp_recv_raw(int sock, uint8_t *buf, int len, int timeout_ms)
{
    uint32_t start = HAL_GetTick();
    int received = 0;
    while (received < len) {
        if (HAL_GetTick() - start > timeout_ms) {
            return -1;
        }
        uint8_t ch;
        while (RingBuf_Read(&ch) && received < len) {
            buf[received++] = ch;
        }

				vTaskDelay(1);
    }
    return received;
}
