#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdalign.h>
#include <threads.h>

#include <wiringPi.h>
#include <wiringSerial.h>

#define BROADCAST_ID 0xFE

#define PFX_COMMAND(PFX)		\
	enum command {				\
		PFX##EEP_WRITE = 0x01,	\
		PFX##EEP_READ,			\
		PFX##RAM_READ,			\
		PFX##RAM_WRITE,			\
		PFX##I_JOG,				\
		PFX##S_JOG,				\
		PFX##STAT,				\
		PFX##ROLLBACK,			\
		PFX##REBOOT				\
	};

#define NORMAL_COMMAND(VOID)	\
	enum command {				\
		EEP_WRITE = 0x01,		\
		EEP_READ,				\
		RAM_READ,				\
		RAM_WRITE,				\
		I_JOG,					\
		S_JOG,					\
		STAT,					\
		ROLLBACK,				\
		REBOOT					\
	};

#define PFX_RAM_REGISTER(PFX)	\
	enum ram_register_map {		\
	PFX##ID	=	0,				\
	PFX##ACK_POLICY,			\
	PFX##ALARM_LED_POLICY,		\
	PFX##TORQUE_POLICY,			\
	PFX##MAX_TEMPERATURE = 5,	\
	PFX##MIN_VOLTAGE,			\
	PFX##MAX_VOLTAGE,			\
	PFX##ACCELERATION_RATIO,	\
	PFX##MAX_ACCELERATION,		\
	PFX##DEAD_ZONE,				\
	PFX##SATURATIOR_OFFSET,		\
	PFX##SATURATOOR_SLOPE,		\
	PFX##PWM_OFFSET,			\
	PFX##MIN_PWM,				\
	PFX##MAX_PWM,				\
	PFX##PWM_THRESHOLD,			\
	PFX##MIN_POSITION,			\
	PFX##MAX_POSITION,			\
	PFX##POSITION_KP,			\
	PFX##POSITION_KD,			\
	PFX##POSITION_KI,			\
	PFX##POSITION_FEED_FORWARD1,\
	PFX##POSITION_FEED_FORWARD2,\
	PFX##LED_BLINK_PERIOD = 38,	\
	PFX##ADC_FAULT_PERIOD,		\
	PFX##PACKET_GARBAGE_PERIOD,	\
	PFX##STOP_DETECTION_PERIOD,	\
	PFX##OVLD_DETECTION_PERIOD,	\
	PFX##STOP_THRESHOLD,		\
	PFX##INPOSITION_MARGIN,		\
	PFX##CAL_DIFFERENCE = 47,	\
	PFX##STATUS_ERROR,			\
	PFX##STATUS_DETAIL,			\
	PFX##TORQUE_CONTROL = 52,	\
	PFX##LED_CONTROL,			\
	PFX##VOLTAGE,				\
	PFX##TEMPERATURE,			\
	PFX##CURRENT_CONTROL_MODE,	\
	PFX##TICK,					\
	PFX##CAL_POSITION,			\
	PFX##ABS_POSITION = 60,		\
	PFX##DIF_POSITION = 62,		\
	PFX##PWM = 64,				\
	PFX##ABS_GOAL_POSITION = 68,\
	PFX##ABS_DSIR_POSITION = 70,\
	PFX##DESIRED_VELOCITY = 72	\
};

#define PFX_NVT_REGISTER(PFX)		\
	enum volatile_register {		\
		PFX##MODEL_NO1 = 1,			\
		PFX##MODEL_NO2,				\
		PFX##VERSION_NO1,			\
		PFX##VERSION_NO2,			\
		PFX##BAUD_RATE,				\
		PFX##ID = 7,				\
		PFX##ACK_POLICY,			\
		PFX##ALARM_LED_POLICY,		\
		PFX##TORQUE_POLICY,			\
		PFX##MAX_TEMPERATURE = 12,	\
		PFX##MIN_VOLTAGE,			\
		PFX##MAX_VOLTAGE,			\
		PFX##ACCELERATION_RATIO,	\
		PFX##DEAD_ZONE,				\
		PFX##SATURATIOR_OFFSET,		\
		PFX##SATURATIOR_SLOPE,		\
		PFX##PWM_OFFSET,			\
		PFX##MIN_PWM,				\
		PFX##MAX_PWM,				\
		PFX##OVERLOAD_PWM_THRESHOLD,\
		PFX##MIN_POSITON,			\
		PFX##MAX_POSTION,			\
		PFX##POSITION_KP,			\
		PFX##POSITION_KI,			\
		PFX##POSITION_FEED_FORWARD1,\
		PFX##POSITIOO_FEED_FORWARD2,\
		PFX##LED_BLINK_PERIOD = 33,	\
		PFX##ADC_FAULT_CHECK_PERIOD,\
		PFX##PACKET_GARBAGE_CHECK,	\
		PFX##STOP_DETECTION_PERIOD,	\
		PFX##OVLD_DETECTION_PERIOD,	\
		PFX##STOP_THRESHOLD,		\
		PFX##INPOSITION_MARGIN,		\
		PFX##CAL_DIFFERENCE = 42,	\
		PFX##STATUS_DIFFERENCE,		\
		PFX##STATUS_ERROR,			\
		PFX##STATUS_DETAIL,			\
		PFX##TORQUE_CONTROL = 47,	\
		PFX##LED_CONTROL,			\
		PFX##VOLTAGE,				\
		PFX##TEMPERATURE,			\
		PFX##CURRENT_CONTROL_MODE,	\
		PFX##TICK,					\
		PFX##CAL_POSITION,			\
		PFX##ABS_POSITION,			\
		PFX##DESIRED_VELOCITY		\
	};

#define PFX_LED_COLOR(PFX)	\
	enum colors {			\
		PFX##GREEN	= 0x01,	\
		PFX##BLUE	= 0x02,	\
		PFX##RED	= 0x04	\
	};


#define PFX_CONTROL_MODE(PFX)	\
	enum control_mode {			\
		PFX##POSITION,			\
		PFX##VELECITY			\
	};

#define PFX_STATUS_ERROR(PFX)			\
	enum status_error {					\
		PFX##EXCEED_VOLTAGE		= 0x01,	\
		PFX##EXCEED_POT			= 0x02,	\
		PFX##EXCEED_TEMPERATURE = 0x04,	\
		PFX##INVALID_PACKET		= 0x08,	\
		PFX##OVLD_DETECTED		= 0x10,	\
		PFX##DV_FAULT_DETECTED	= 0x20,	\
		PFX##EEP_REG_DISTORTED	= 0x40	\
	};

#define PFX_STATUS_DETAIL(PFX)			\
	enum status_detail {				\
		PFX##MOVING_FLAG		= 0x01,	\
		PFX##INPOSITION_FLAG	= 0x02,	\
		PFX##CHECKSUM_FLAG		= 0x04,	\
		PFX##UNKOWN_COMMAND		= 0x08,	\
		PFX##EXCEED_REG_RANGE	= 0x10,	\
		PFX##GARBAGE_DETECTED	= 0x20,	\
		PFX##MONITOR_ON_FLAG	- 0x40	\
	};

PFX_COMMAND(HRX_CMD_);
PFX_RAM_REGISTER(HRX_RAM_REG_);
PFX_NVT_REGISTER(HRX_NVT_REG_);
PFX_LED_COLOR(HRX_LED_);
PFX_CONTROL_MODE(HRX_CONTROL_);
PFX_STATUS_ERROR(HRX_STAT_ERR_);
PFX_STATUS_DETAIL(HRX_STAT_DTL_);

typedef uint8_t byte;

struct packet {
	byte header[2];
	byte size;
	byte id;
	byte command;
	byte check[2];
};

_Noreturn void error_handling(const char *format, ...);

int main(int argc, char *argv[])
{
	printf("%zu \n", sizeof(struct packet));

	return 0;
}

_Noreturn void error_handling(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	vfprintf(stderr, fmt, ap);

	va_end(ap);

	exit(1);
}

void hrx_calc_checksum(struct packet *pk, int n, byte data[n])
{
	byte result = pk->size;
	
	result ^= pk->id;
	result ^= pk->command;
	
	for (int i = 0; i < n; i++)
		result ^= data[i];

	result &= 0xFE;

	pk->check[0] = result;
	pk->check[1] = (~result) & 0xFE;
}


