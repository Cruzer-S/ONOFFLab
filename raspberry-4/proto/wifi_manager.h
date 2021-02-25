#ifndef WIFI_MANAGER_H__
#define WIFI_MANAGER_H__

#define WPA_DIRECTORY	"/etc/wpa_supplicant/wpa_supplicant.conf"

#define SSID_SIZ		1024
#define PSK_SIZ			1024

#include <stdio.h>		// FILE, fprintf
#include <stdlib.h>		// system function
#include <stdbool.h>	// bool, true, false

bool change_wifi(const char *ssid, const char *psk);

#endif
