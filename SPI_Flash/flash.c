#include "flash.h"
#include "bsp_flash.h"
#include <string.h>
#include <stdio.h>
#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

// ===================== 新增：Flash 全局互斥锁 =====================
SemaphoreHandle_t flash_mutex = NULL;

// ===================== 新增：日志区写入偏移（循环写标记）=====================
 uint32_t log_write_offset = 0U;

extern UART_HandleTypeDef huart1;

typedef struct {
    uint32_t magic;
    uint32_t timestamp;
    uint16_t data_len;
    uint8_t  data[CACHE_ENTRY_SIZE - 12];
} CacheEntry_t;

static uint32_t write_addr = CACHE_START_ADDR;
static uint32_t read_addr  = CACHE_START_ADDR;
static uint8_t  initialized = 0;

/**
 * @brief  分区读数据
 * @param  part_base 分区基地址
 * @param  offset    分区内偏移
 * @param  buf       接收缓冲区
 * @param  len       读取长度
 * @retval 0成功 1失败
 */
uint8_t Flash_Part_Read(uint32_t part_base, uint32_t offset, uint8_t *buf, uint16_t len)
{
    if(buf == NULL) return 1;
    uint32_t real_addr = part_base + offset;

    SPI_FLASH_BufferRead(buf, real_addr, len);

    return 0;
}

/**
 * @brief  分区写数据
 * @param  part_base 分区基地址
 * @param  offset    分区内偏移
 * @param  buf       待写数据
 * @param  len       写入长度
 * @retval 0成功 1失败
 */
uint8_t Flash_Part_Write(uint32_t part_base, uint32_t offset, uint8_t *buf, uint16_t len)
{
    if(buf == NULL) return 1;
    uint32_t real_addr = part_base + offset;

    SPI_FLASH_BufferWrite(buf, real_addr, len);

    return 0;
}

void Log_Write(Log_Level level, const char *str)
{

    Log_Obj log_buf = {0};
    uint16_t str_len;

    log_buf.time_stamp = HAL_GetTick();
    log_buf.level = level;
    log_buf.magic = LOG_MAGIC;

    str_len = strlen(str);
    if(str_len > LOG_CONTENT_LEN - 1U)
        str_len = LOG_CONTENT_LEN - 1U;
    memcpy(log_buf.content, str, str_len);

    if((log_write_offset + LOG_OBJ_SIZE) > LOG_PART_SIZE)
    {
        log_write_offset = 0U;
    }

    Flash_Part_Write(LOG_PART_ADDR, log_write_offset, (uint8_t *)&log_buf, LOG_OBJ_SIZE);

    log_write_offset += LOG_OBJ_SIZE;
}

/**
 * @brief  读取单条日志
 * @param  log_offset 日志区内偏移(读指针)
 * @param  log_obj    日志结构体接收缓存
 * @retval 0=有效日志 1=无效/空日志
 */
uint8_t Log_ReadOne(uint32_t log_offset, Log_Obj *log_obj)
{
    if(log_obj == NULL) return 1;

    Flash_Part_Read(LOG_PART_ADDR, log_offset, (uint8_t *)log_obj, LOG_OBJ_SIZE);

    if(log_obj->magic != LOG_MAGIC)
        return 1;

    return 0;
}

void Log_EraseAll(void)
{
    uint32_t sec_addr;

    for(sec_addr = LOG_PART_ADDR; sec_addr < LOG_PART_ADDR + LOG_PART_SIZE; sec_addr += 4096)
    {
        SPI_FLASH_SectorErase(sec_addr);
        SPI_FLASH_WaitForWriteEnd();
    }

    log_write_offset = 0;
}

void FlashCache_Init(void)
{

	 log_write_offset = 0;
    CacheEntry_t entry;
    uint32_t addr = CACHE_START_ADDR;

    write_addr = CACHE_START_ADDR;
    while (addr < CACHE_END_ADDR) {
        SPI_FLASH_BufferRead((uint8_t *)&entry, addr, sizeof(CacheEntry_t));

        if (entry.magic == MAGIC_VALID) {
            write_addr = addr + sizeof(CacheEntry_t);
        } else {
            break;
        }
        addr += sizeof(CacheEntry_t);
    }

    if (write_addr >= CACHE_END_ADDR) {
        write_addr = CACHE_START_ADDR;
    }

    read_addr = CACHE_START_ADDR;
    initialized = 1;
}

int FlashCache_Write(const char *json, uint16_t len)
{
    if (!initialized) return -1;
    if (len > CACHE_ENTRY_SIZE - 12) return -2;

		    // ========== 加锁 ==========

    CacheEntry_t entry;
		memset(&entry, 0xFF, sizeof(entry));
    entry.magic     = MAGIC_VALID;
    entry.timestamp = HAL_GetTick();
    entry.data_len  = len;
    memcpy(entry.data, json, len);

    if ((write_addr +sizeof(CacheEntry_t))>= CACHE_END_ADDR) {
        write_addr = CACHE_START_ADDR;
        SPI_FLASH_SectorErase(write_addr);
    }

    if (write_addr % CACHE_SECTOR_SIZE == 0) {
        SPI_FLASH_SectorErase(write_addr);
    }

    SPI_FLASH_BufferWrite((uint8_t *)&entry, write_addr, sizeof(CacheEntry_t));
    write_addr += sizeof(CacheEntry_t);

		    // ========== 解锁 ==========

    return 0;
}

int FlashCache_ReadAndConsume(char *json, uint16_t *len)
{
    if (!initialized) return -1;

		    // ========== 加锁 ==========

    CacheEntry_t entry;
    if (read_addr >= write_addr) {
        return -2;
    }

    SPI_FLASH_BufferRead((uint8_t *)&entry, read_addr, sizeof(CacheEntry_t));
    if (entry.magic != MAGIC_VALID) {
        read_addr = write_addr;
        return -2;
    }

    memcpy(json, entry.data, entry.data_len);
    json[entry.data_len] = '\0';
    *len = entry.data_len;

    entry.magic = MAGIC_EMPTY;
    SPI_FLASH_WriteEnable();
    SPI_FLASH_BufferWrite((uint8_t *)&entry, read_addr, sizeof(CacheEntry_t));

    read_addr += sizeof(CacheEntry_t);

		    // ========== 解锁 ==========

    return 0;
}

// ======================================================================
int FlashCache_HasData(void)
{
    if (!initialized) return 0;

    static int total = 0;
    if (total >= 5) {
        total = 0;
        return 0;
    }

    if (read_addr < write_addr) {
        total++;
        return 1;
    }

    total = 0;
    return 0;
}

void my_clear()
{
	    write_addr = CACHE_START_ADDR;
    read_addr  = CACHE_START_ADDR;
}

void FlashCache_Clear(void)
{
		    // ========== 加锁 ==========

    for (uint32_t addr = CACHE_START_ADDR; addr < CACHE_END_ADDR; addr += CACHE_SECTOR_SIZE) {
        SPI_FLASH_SectorErase(addr);
    }
    write_addr = CACHE_START_ADDR;
    read_addr  = CACHE_START_ADDR;
    initialized = 1;

		    // ========== 解锁 ==========

}

void MQTT_PublishJson(const char *json)
{
    uint8_t buf[256];
    int pos = 0;
    buf[pos++] = 0x32;

	uint16_t topic_len = 5;
  uint16_t msg_len = strlen(json);
	uint16_t rem = 2 + topic_len + 2 + msg_len;

    do {
        uint8_t b = rem % 128;
        rem /= 128;
        if (rem > 0) b |= 0x80;
        buf[pos++] = b;
    } while (rem > 0);

    buf[pos++] = 0;
    buf[pos++] = topic_len;
    memcpy(&buf[pos], "stm32", 5);
    pos += 5;

    buf[pos++] = 0;
    buf[pos++] = 1;

    memcpy(&buf[pos], json, msg_len);
    pos += msg_len;

    HAL_UART_Transmit(&huart1, buf, pos, 5000);
    vTaskDelay(500);
}

// ======================================================================
void FlashCache_MarkSent(void)
{
    if (!initialized) return;

    uint32_t cur = read_addr - sizeof(CacheEntry_t);
    if (cur < CACHE_START_ADDR) return;

    CacheEntry_t entry;
    SPI_FLASH_BufferRead((uint8_t *)&entry, cur, sizeof(CacheEntry_t));

    if (entry.magic == MAGIC_VALID) {
			entry.magic = MAGIC_EMPTY;
        SPI_FLASH_WriteEnable();
        SPI_FLASH_BufferWrite((uint8_t *)&entry, cur, sizeof(CacheEntry_t));
    }
}

int FlashCache_Read(char *json, uint16_t *len)
{
    if (!initialized) return -1;

    static int read_cnt = 0;
    if (read_cnt >= 5) {
        read_cnt = 0;
        read_addr = write_addr;
        return -2;
    }

    if (read_addr >= write_addr) {
        read_cnt = 0;
        return -2;
    }

		    // ========== 加锁 ==========

    CacheEntry_t entry;
    SPI_FLASH_BufferRead((uint8_t *)&entry, read_addr, sizeof(CacheEntry_t));

    if (entry.magic != MAGIC_VALID) {
        read_addr = write_addr;
        read_cnt = 0;
        return -2;
    }

    memcpy(json, entry.data, entry.data_len);
    json[entry.data_len] = '\0';
    *len = entry.data_len;

    read_addr += sizeof(CacheEntry_t);
    read_cnt++;

		    // ========== 解锁 ==========

    return 0;
}
