#include "wifi_manager.h"

#define WPA_DIRECTORY	"/etc/wpa_supplicant/wpa_supplicant.conf"

static bool refresh_wifi(void);

extern bool change_wifi(const char *ssid, const char *psk)/*{{{*/
{
	FILE *fp;
	const char *form = {
		"country=US\n"
		"ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev\n"
		"update_config=1\n"
		"\n"
		"network={\n"
		"\tssid=\"%s\"\n"
		"\tscan_ssid=1\n"
		"\tpsk=\"%s\"\n"
		"\tkey_mgmt=WPA-PSK\n"
		"}"
	};

	fp = fopen(WPA_DIRECTORY, "w");
	if (fp == NULL)
		return false;

	fprintf(fp, form, ssid, psk);

	fclose(fp);

	if (!refresh_wifi())
		return false;

	return true;
}/*}}}*/

static bool refresh_wifi(void)/*{{{*/
{
	// alert changing of /etc/wpa_supplicant/wpa_supplicant.conf
	if (system("sudo systemctl daemon-reload") == EXIT_FAILURE)
		return false;

	// restart dhcpcd which is parent of wpa_supplicant daemon 
	if (system("sudo systemctl restart dhcpcd") == EXIT_FAILURE)
		return false;

	return true;
}/*}}}*/

