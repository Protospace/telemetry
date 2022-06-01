#include <SoftwareSerial.h>

SoftwareSerial mySerial(PB3, PB2);

void setup() {
	mySerial.begin(9600);
	pinMode(PB3, INPUT);
	pinMode(PB2, OUTPUT);
	digitalWrite(PB2, LOW);
}

bool ledState = false;
char junk = 'a';

void loop() {
	if (mySerial.available()) {
		ledState = !ledState;
		digitalWrite(PB2, ledState);

		while (mySerial.available()) {
			junk = mySerial.read();
		}
	}
}
