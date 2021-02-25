#ifndef BLUETOOTH_HANDLER__
#define BLUETOOTH_HANDLER__

int check_initiate(int serial_port, int key);
int scratch_data(int serial_port, char *ssid, char *psk);

#endif
