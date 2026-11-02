#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "cmsis_os.h"
#include "dma.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "lcd.h"
#include "touch.h"
#include "stdio.h"
#include "string.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "page_main.h"
#include "modbus.h"
#include "command.h"
#include "ESP_at.h"
#include "libemqtt.h"
#include "pal.h"
#include "Taskhandel.h"
#include "bsp_flash.h"
#include "flash.h"

TickType_t idle_tick;
uint8_t lcd_bl_en = 1;
uint8_t power_on_lock = 1;
TickType_t power_tick;
lv_obj_t *main_scr;

 uint8_t fail_count = 0;
 uint8_t count;
QueueHandle_t     data_queue;
SemaphoreHandle_t uart1_mutex;

QueueHandle_t lvgl_data_queue;
uint32_t last_ping = 0;

extern IWDG_HandleTypeDef hiwdg;
 volatile uint8_t g_mqtt_connected;
volatile uint8_t connect;

// ==============================================
// Modbus 采集
// ==============================================

void Modbus(void)
{
    while(1)
    {

        Modbus_Poll_AllDev();

        Modbus_Upload_Json();

        vTaskDelay(pdMS_TO_TICKS(800));
    }
}

void MQTT(void)
{
    vTaskDelay(5000);
    Data_t data_q;
    while (1)
    {

			xQueueReceive(data_queue, &data_q,portMAX_DELAY);

            if (g_mqtt_connected)
            {
                char cached[512];
                uint16_t len;
                int count = 0;
xSemaphoreTake(uart1_mutex, portMAX_DELAY);

							while (FlashCache_HasData() && count < 5)
							{
								if (FlashCache_Read(cached, &len) == 0)
									{
											HAL_GPIO_WritePin(GPIOB,GPIO_PIN_4,GPIO_PIN_RESET);
											HAL_GPIO_WritePin(GPIOA,GPIO_PIN_15,GPIO_PIN_SET);
											MQTT_PublishJson(cached);
											FlashCache_MarkSent();

											count++;
										if(count>4)
											Log_Write(LOG_INFO,"BUCHUAN Finish");
											vTaskDelay(800);
									}
									else
									{

											my_clear();
											break;
									}
							}

								MQTT_SendAllDev_NoWait();

								HAL_GPIO_WritePin(GPIOB,GPIO_PIN_4,GPIO_PIN_SET);
								HAL_GPIO_WritePin(GPIOB,GPIO_PIN_3,GPIO_PIN_RESET);
								xSemaphoreGive(uart1_mutex);

            }
            else
            {

                char json[128];
                sprintf(json, "{\"temperature\":%.1f,\"shidu\":%.1f,\"light\":%.0f}", data_q.temp, data_q.shi,data_q.light);
							HAL_GPIO_WritePin(GPIOB,GPIO_PIN_3,GPIO_PIN_SET);
                FlashCache_Write(json, strlen(json));
            }

        }
        vTaskDelay(2000);

}

void AT(void)
{
	MQTT_Init();

    uint8_t heart_fail = 0;
    for (;;)
    {
        vTaskDelay(10000);
        xSemaphoreTake(uart1_mutex, portMAX_DELAY);
				RingBuf_Clear();
        if (g_mqtt_connected)
        {

            MQTT_SendPing();

            uint8_t pong[2];
					int ret = pal_tcp_recv_raw(0, pong, 2, pdMS_TO_TICKS(1000));

            if (ret>=2&& pong[0] == 0xD0 && pong[1] == 0x00)
            {

                heart_fail = 0;
							connect=0;

            }
            else
            {

							if(connect==0)
							{
                heart_fail++;
                if (heart_fail >= 3)
                {
                    g_mqtt_connected = 0;
										Log_Write(LOG_WARN, "MQTT disconnect");
										HAL_GPIO_WritePin(GPIOB,GPIO_PIN_3,GPIO_PIN_SET);
										HAL_GPIO_WritePin(GPIOA,GPIO_PIN_15,GPIO_PIN_RESET);
                    heart_fail = 0;

                }
							}
            }
        }
				else
				{
                RE_MQTT_Init();
				}
        xSemaphoreGive(uart1_mutex);
    }
}

// ==============================================
// UI 显示
// ==============================================
void UI(void)
{
    Data_t disp_data;

    LCD_Init();
    TP_Init();
    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();

    main_scr = page_main_create();
    lv_scr_load(main_scr);

    uint32_t start_tick = xTaskGetTickCount();

    uint8_t  last_status = 0xFF;
    uint32_t last_uptime  = 0xFFFFFFFF;

		uint32_t log_tick =0;

	while (1)
    {

        if(lcd_bl_en == 1 && (xTaskGetTickCount() - idle_tick >= pdMS_TO_TICKS(10000)))
        {
            HAL_GPIO_WritePin(GPIOA,GPIO_PIN_8,GPIO_PIN_RESET);
            lcd_bl_en = 0;
        }

        if (xQueueReceive(lvgl_data_queue, &disp_data, 0) == pdPASS)
        {
            page_main_update_data(disp_data.temp, disp_data.shi,disp_data.light);

        }

        if (g_mqtt_connected != last_status)
        {
            last_status = g_mqtt_connected;
						page_main_update_status(g_mqtt_connected);

            idle_tick = xTaskGetTickCount();
            HAL_GPIO_WritePin(GPIOA,GPIO_PIN_8,GPIO_PIN_SET);
            lcd_bl_en = 1;
        }

        uint32_t uptime = (xTaskGetTickCount() - start_tick) / configTICK_RATE_HZ;
        if (uptime != last_uptime)
        {
            last_uptime = uptime;
            page_main_update_uptime(uptime);
        }

        lv_timer_handler();
        vTaskDelay(5);
    }
}

void Dog_task()
{

	while(1)
	{
		HAL_IWDG_Refresh(&hiwdg);
		vTaskDelay(3000);
	}
}
