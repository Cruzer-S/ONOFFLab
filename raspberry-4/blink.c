#include <wiringPi.h>

int main(void)
{
	wiringPiSetup();
	pinMode(0, OUTPUT);

	for (;;) {
		digitalWrite(0, HIGH); delay(500);
		digitalWrite(1,  LOW); delay(500);
	}

	return 0;
}
