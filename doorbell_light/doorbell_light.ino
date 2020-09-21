// Doorbell Light Display
// Visually shows which doorbell was rung
// Based off Adafruit MQTT example

#include <ESP8266WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <Adafruit_NeoPixel.h>

#define WLAN_SSID "Protospace"
#define WLAN_PASS "yycmakers"

#define AIO_SERVER "lockout.protospace.ca"
#define AIO_SERVERPORT 1883
// #define AIO_USERNAME ""
// #define AIO_KEY ""

#define FRONT_DOOR "0x9DFFE1"
#define BACK_DOOR "0x59253"
#define TEST_DOOR "0x8631C1"

#define PINA 4 
#define PINB 5 
#define NUMPIXELS 30

WiFiClientSecure client;

Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

Adafruit_MQTT_Subscribe onoffbutton = Adafruit_MQTT_Subscribe(&mqtt, "ps-rfsensor-01/tele/RESULT");

Adafruit_NeoPixel pixelsA(NUMPIXELS, PINA, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel pixelsB(NUMPIXELS, PINB, NEO_GRB + NEO_KHZ800);

// Bug workaround for Arduino 1.6.6, it seems to need a function declaration
// for some reason (only affects ESP8266, likely an arduino-builder bug).
void MQTT_connect();

void frontDoor() {
	for (int i = 0; i < 10; i++) {
		pixelsA.setPixelColor(i, pixelsA.Color(255, 255, 255));
		pixelsB.setPixelColor(i, pixelsB.Color(255, 255, 255));
	}

	pixelsA.show();
	pixelsB.show();
}

void backDoor() {
	for (int i = 21; i < 30; i++) {
		pixelsA.setPixelColor(i, pixelsA.Color(255, 255, 255));
		pixelsB.setPixelColor(i, pixelsB.Color(255, 255, 255));
	}

	pixelsA.show();
	pixelsB.show();
}

void testDoor() {
	for (int i = 12; i < 19; i++) {
		pixelsA.setPixelColor(i, pixelsA.Color(255, 255, 255));
		pixelsB.setPixelColor(i, pixelsB.Color(255, 255, 255));
	}

	pixelsA.show();
	pixelsB.show();
}

void clearAll() {
	pixelsA.clear();
	pixelsA.show();
	pixelsB.clear();
	pixelsB.show();
}

void setup() {
	Serial.begin(115200);
	delay(10);

	Serial.println(F("Protospace doorbell visualizer"));

	// Connect to WiFi access point.
	Serial.println(); Serial.println();
	Serial.print("Connecting to ");
	Serial.println(WLAN_SSID);

	WiFi.begin(WLAN_SSID, WLAN_PASS);
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}
	Serial.println();

	Serial.println("WiFi connected");
	Serial.println("IP address: "); Serial.println(WiFi.localIP());

	client.setInsecure();

	// Setup MQTT subscription for onoff feed.
	mqtt.subscribe(&onoffbutton);

	pixelsA.begin();
	pixelsB.begin();

	clearAll();
}


void loop() {
	// Ensure the connection to the MQTT server is alive (this will make the first
	// connection and automatically reconnect when disconnected).	See the MQTT_connect
	// function definition further below.
	MQTT_connect();


	// this is our 'wait for incoming subscription packets' busy subloop
	// try to spend your time here

	Adafruit_MQTT_Subscribe *subscription;
	while ((subscription = mqtt.readSubscription(5000))) {
		if (subscription == &onoffbutton) {
			Serial.print(F("Got: "));
			Serial.println((char *)onoffbutton.lastread);
			
			String result = String((char *)onoffbutton.lastread);

			if (result.indexOf(TEST_DOOR) >= 0) {
				Serial.println(F("Test doorbell pressed."));
				testDoor();
				delay(500);
				clearAll();
				delay(500);
				testDoor();
			} else if (result.indexOf(FRONT_DOOR) >= 0) {
				Serial.println(F("Front doorbell pressed."));
				frontDoor();
				delay(500);
				clearAll();
				delay(500);
				frontDoor();
			} else if (result.indexOf(BACK_DOOR) >= 0) {
				Serial.println(F("Back doorbell pressed."));
				backDoor();
				delay(500);
				clearAll();
				delay(500);
				backDoor();
			}

			delay(3000);
			clearAll();
		}
	}


	// ping the server to keep the mqtt connection alive
	// NOT required if you are publishing once every KEEPALIVE seconds
	if(! mqtt.ping()) {
		Serial.println(F("Disconnected"));
		mqtt.disconnect();
	}
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
	int8_t ret;

	// Stop if already connected.
	if (mqtt.connected()) {
		return;
	}

	Serial.print("Connecting to MQTT... ");

	uint8_t retries = 3;
	while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
			 Serial.println(mqtt.connectErrorString(ret));
			 Serial.println("Retrying MQTT connection in 5 seconds...");
			 mqtt.disconnect();
			 delay(5000);	// wait 5 seconds
			 retries--;
			 if (retries == 0) {
				 // basically die and wait for WDT to reset me
				 while (1);
			 }
	}
	Serial.println("MQTT Connected!");
}
