#ifndef _SERVO_HRX_H_
#define _SERVO_HRX_H_

#include <stdbool.h>
#include <wiringPi.h>
#include <wiringSerial.h>

#include "hrx_def.h"

PFX_COMMAND(HRX_CMD_);
PFX_RAM_REGISTER(HRX_RAM_REG_);
PFX_NVT_REGISTER(HRX_NVT_REG_);
PFX_LED_COLOR(HRX_LED_);
PFX_CONTROL_MODE(HRX_CONTROL_);
PFX_STATUS_ERROR(HRX_STAT_ERR_);
PFX_STATUS_DETAIL(HRX_STAT_DTL_);

struct motor hrx_create_motor(byte id);
void hrx_delete_motor(struct motor *motor);

struct motor hrx_connect_serial(struct motor *motor, int serial);

void hrx_set_one_position(struct motor motor, int position, unsigned time, enum color color);
bool hrx_send_data(struct motor motor, enum command command, int n, byte data[n]);

int open_serial(int baud_rate);
void close_serial(int serial);


#endif
