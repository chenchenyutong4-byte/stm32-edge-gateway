#ifndef FLASH_H
#define FLASH_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "semphr.h"

// ===================== 1. W25Q64 分区定义（固定不变）=====================

#define LOG_PART_ADDR       0x000000U
#define LOG_PART_SIZE       (0x400000U-1)

#define CACHE_START_ADDR    (4 * 1024 * 1024)
#define CACHE_END_ADDR      (8 * 1024 * 1024)
#define CACHE_SECTOR_SIZE   4096
#define CACHE_ENTRY_SIZE    128

#define MAGIC_VALID         0xDEADBEEF
#define MAGIC_EMPTY         0xFFFFFFFF

#define LOG_CONTENT_LEN     64U
#define LOG_MAGIC           0x55AAU

// ===================== 2. 日志等级 & 日志结构体 =====================
typedef enum
{
    LOG_INFO  = 0,
    LOG_WARN  = 1,
    LOG_ERROR = 2
} Log_Level;

typedef struct
{
    uint32_t    time_stamp;
    Log_Level   level;
    uint8_t     content[LOG_CONTENT_LEN];
    uint16_t    magic;
} Log_Obj;

#define LOG_OBJ_SIZE        sizeof(Log_Obj)

// ===================== 3. 全局互斥锁（Flash 全局保护）=====================
extern SemaphoreHandle_t flash_mutex;

// ===================== 4. 分区通用读写接口（新增）=====================
uint8_t Flash_Part_Read(uint32_t part_base, uint32_t offset, uint8_t *buf, uint16_t len);
uint8_t Flash_Part_Write(uint32_t part_base, uint32_t offset, uint8_t *buf, uint16_t len);

// ===================== 5. 日志接口（新增）=====================
void Log_Write(Log_Level level, const char *str);
uint8_t Log_ReadOne(uint32_t log_offset, Log_Obj *log_obj);

// ===================== 6. 原有环形缓存接口（全部保留）=====================
extern volatile uint8_t g_mqtt_connected;

void FlashCache_Init(void);
int  FlashCache_Write(const char *json, uint16_t len);
int  FlashCache_Read(char *json, uint16_t *len);
int  FlashCache_HasData(void);
void FlashCache_Clear(void);
void MQTT_PublishJson(const char *json);
void MQTT_SendTempHumi_WithCache(float temp, float humi);

void FlashCache_MarkSent(void);
int FlashCache_ReadAndConsume(char *json, uint16_t *len);
void my_clear();
#endif
