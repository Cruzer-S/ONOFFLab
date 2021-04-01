#ifndef DEVICE_MANAGER_H__
#define DEVCIE_MANAGER_H__

#include <time.h>

#include UNION_LIBRARY(ipc_manager.h)

#include "task_manager.h"
#include "wifi_manager.h"

#define BOAD_RATE			9600
#define SERIAL_DEVICE		"/dev/ttyS0"

#define DEVICE_ID			0x00000001
#define DEVICE_KEY			"hello12"

#define BLUETOOTH_NAME_MAX	32

struct
{
	int status;
	union {
		int error_code;
		struct {
			int completion;
			time_t estimate;
			char filename[TASK_NAME_SIZE];
		} task;
	};
} printer_status;

struct
{
	struct {
		struct {
			float room;
			float bed;
			float nozzle;
		} temperature;

		float speed;
		float layer;

		struct {
			float x, y;
			float zoffset;
		} relative;
	} option;

	int system;
	int screenshoot;
	int lamp;
} printer_behavior;

struct printer_information
{
	char *product;
	const int unique;
	char key[DEVICE_KEY_SIZE];
	struct {
		char ssid[SSID_SIZ];
		char psk[PSK_SIZ];
	} wifi;
	char bluetooth_name[BLUETOOTH_NAME_MAX];
} printer_information = {
	.product = "ONOFFLAB 3D Printer",
	.unique = 1,
	.key = DEVICE_KEY
};

#endif
