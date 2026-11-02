#include "command.h"
#include "usart.h"
#include "stm32f4xx.h"
#include "dma.h"
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
#include "cmsis_os2.h"

extern uint8_t a[256];
extern DMA_HandleTypeDef hdma_usart1_rx;

uint8_t buffer[BUFFER_SIZE];

uint8_t readIndex = 0;

uint8_t writeIndex = 0;

/**
 * @brief  清空环形缓冲区（重置读写指针）
 */
void RingBuf_Clear(void)
{
    readIndex = 0;
    writeIndex = 0;
}

/**
* @brief 增加读索引
* @param length 要增加的长度
*/
void Command_AddReadIndex(uint8_t length) {
    readIndex += length;
    readIndex %= BUFFER_SIZE;
}

/**
* @brief 读取第i位数据 超过缓存区长度自动循环
* @param i 要读取的数据索引
*/

uint8_t Command_Read(uint8_t i) {
    uint8_t index = i % BUFFER_SIZE;
    return buffer[index];
}

uint8_t RingBuf_Read(uint8_t *out) {
    if (readIndex == writeIndex) {
        return 0;
    }
    *out = buffer[readIndex];
    readIndex = (readIndex + 1) % BUFFER_SIZE;
    return 1;
}

uint8_t Command_GetLength() {
    return (writeIndex + BUFFER_SIZE - readIndex) % BUFFER_SIZE;
}

/**
* @brief 计算缓冲区剩余空间
* @return 剩余空间
* @retval 0 缓冲区已满
* @retval 1~BUFFER_SIZE-1 剩余空间
* @retval BUFFER_SIZE 缓冲区为空
*/
uint8_t Command_GetRemain() {
    return BUFFER_SIZE - Command_GetLength();
}

/**	不会覆盖旧数据 满后停止写入
* @brief 向缓冲区写入数据
* @param data 要写入的数据指针
* @param length 要写入的数据长度
* @return 写入的数据长度
*/

uint8_t Command_Write(uint8_t *data, uint8_t len)
{

    if (data == NULL || len == 0)
        return 0;

    uint16_t remain = Command_GetRemain();
    if (remain < len)
        return 0;

    osKernelLock();

    uint16_t idx = writeIndex;

    uint16_t part1 = BUFFER_SIZE - idx;
    if (len <= part1)
    {
        memcpy(buffer + idx, data, len);
        writeIndex = (idx + len) % BUFFER_SIZE;
    }
    else
    {
        memcpy(buffer + idx, data, part1);
        memcpy(buffer, data + part1, len - part1);
        writeIndex = (len - part1) % BUFFER_SIZE;
    }

    osKernelUnlock();
    return len;
}

extern uint8_t screen_off_flag;
/**
* @brief 从缓冲区读取所有未处理的数据（不解析指令帧）
* @param command 存储读取数据的指针
* @return 实际读取的字节数（0 表示缓冲区为空）
*/
uint8_t Command_GetCommand(uint8_t *command) {
    uint8_t len = Command_GetLength();
    if (len == 0) {
        return 0;
    }

    for (uint8_t i = 0; i < len; i++) {
        command[i] = Command_Read(readIndex + i);
    }
    Command_AddReadIndex(len);
    return len;
}

extern TickType_t idle_tick;
extern uint8_t lcd_bl_en ;

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
	if(huart==&huart1)
	{
		Command_Write(a,Size);

		HAL_UARTEx_ReceiveToIdle_DMA(&huart1,a,sizeof(a));
		__HAL_DMA_DISABLE_IT(&hdma_usart1_rx,DMA_IT_HT);
	}
}
