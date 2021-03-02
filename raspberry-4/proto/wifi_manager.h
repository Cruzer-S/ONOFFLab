#ifndef WIFI_MANAGER_H__
#define WIFI_MANAGER_H__

#define SSID_SIZ		32
#define PSK_SIZ			20

#include <stdio.h>		// FILE, fprintf
#include <stdlib.h>		// system function
#include <stdbool.h>	// bool, true, false

bool change_wifi(const char *ssid, const char *psk);

#endif
