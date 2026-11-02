#include "esp_at.h"
#include "command.h"
#include "usart.h"
#include <string.h>
#include <stdio.h>

extern uint8_t buffer[BUFFER_SIZE];

static void ESP_ClearRxBuf(void) {
    uint8_t dummy;
    while (RingBuf_Read(&dummy));
}

ESP_Status_t ESP_SendCmd(const char *cmd, const char *expected_ack, uint32_t timeout_ms)
{

    ESP_ClearRxBuf();

    HAL_UART_Transmit(&huart1, (uint8_t *)cmd, strlen(cmd), 1000);

    char line[512];
    uint16_t line_pos = 0;
    uint32_t start_tick = HAL_GetTick();

    while ((HAL_GetTick() - start_tick) < timeout_ms) {
        uint8_t ch;
			while (RingBuf_Read(&ch)) {
            start_tick = HAL_GetTick();

            if (ch == '\n') {

                line[line_pos] = '\0';

                if (expected_ack != NULL && strstr(line, expected_ack) != NULL) {
                    return ESP_OK;
                }

                if (strstr(line, "ERROR") != NULL) {
                    return ESP_ERROR;
                }

                line_pos = 0;
            }
            else if (ch != '\r') {

                if (line_pos < sizeof(line) - 1) {
                    line[line_pos++] = ch;
                }
            }
        }
    }

    return ESP_TIMEOUT;
}

ESP_Status_t ESP_SendCmd_OK(const char *cmd, uint32_t timeout_ms)
{
    return ESP_SendCmd(cmd, "OK", timeout_ms);
}

/**
 * @brief  等待环形缓冲区中出现指定关键字（超时返回）
 * @param  keyword   要匹配的关键字（字符串）
 * @param  timeout_ms 超时时间（毫秒）
 * @return 1-匹配成功，0-超时未匹配
 */
