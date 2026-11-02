#ifndef TASKHANDEL_H
#define TASKHANDEL_H
#include "stdint.h"

void Dog_task();
void MQTT();
void AT();
void UI();
void Modbus();
void page_log_refresh(void);
extern  volatile uint8_t connect;

typedef struct {
    float temp;
    float shi;
	 float light;
} Data_t;

#endif
