#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoMqttClient.h>
#include <WEMOS_SHT3X.h>
#include "DSM501.h"
#include "secrets.h"
#include "lets_encrypt_ca.h"

#define DSM501_PM1_0    D7
#define DSM501_PM2_5    D6
#define SAMPLE_TIME     60 // seconds
#define MAX_FAILS       SAMPLE_TIME * 10 * 5  // ~five minutes

WiFiClientSecure wc;
MqttClient mqttClient(wc);
SHT3X sht30(0x45);
DSM501 dsm501;

void (* resetFunc) (void) = 0;

const char broker[] = "webhost.protospace.ca";
int        port     = 8883;

bool firstIgnored = false;
long failCount = 0;

void setup() {

	Serial.begin(115200);
	// Serial.setDebugOutput(true);

	Serial.println();
	Serial.println();
	Serial.println();

	delay(1000);

	WiFi.mode(WIFI_STA);
	WiFi.begin("Protospace", "yycmakers");
	Serial.print("Connecting to WiFi");
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}
	Serial.println("");

	// Synchronize time useing SNTP. This is necessary to verify that
	// the TLS certificates offered by the server are currently valid.
	Serial.print("Setting time using SNTP");
	configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");
	time_t now = time(nullptr);
	while (now < 8 * 3600 * 2) {
		delay(500);
		Serial.print(".");
		now = time(nullptr);
	}
	Serial.println("");
	struct tm timeinfo;
	gmtime_r(&now, &timeinfo);
	Serial.print("Current time: ");
	Serial.print(asctime(&timeinfo));

	wc.setCACert_P(lets_encrypt_ca, lets_encrypt_ca_len);

	mqttClient.setUsernamePassword(MQTT_USERNAME, MQTT_PASSWORD);

	// Initialize DSM501:
	//           PM1.0 pin     PM2.5 pin     sampling duration in seconds
	dsm501.begin(DSM501_PM1_0, DSM501_PM2_5, SAMPLE_TIME);

	// Wait 120s for DSM501 to warm up
	Serial.println("Wait 120s for DSM501 to warm up");
	for (int i = 1; i <= 120; i++)
	{
		delay(1000); // 1s
		Serial.print(i);
		Serial.println(" s (wait 120s for DSM501 to warm up)");
	}

	Serial.println("DSM501 is ready!");
	Serial.println();
}

bool sendSample() {
	if (!dsm501.update()) {
		return false;
	}

	if (sht30.get()) {
		Serial.printf("[TEMP] Unable to get temperature\n");
		return false;
	}

	if (WiFi.status() != WL_CONNECTED) {
		Serial.printf("[WIFI] Not connected\n");
		return false;
	}

	Serial.printf("[MQTT] Connecting to broker...\n");

	if (!mqttClient.connect(broker, port)) {
		Serial.print("[MQTT] Connection failed! Error code = ");
		Serial.println(mqttClient.connectError());
		return false;
	}

	String temp = String(sht30.cTemp);
	Serial.print("[TEMP] Temperature: " + temp + " C\n");
	String pm25 = String(dsm501.getConcentration());
	Serial.print("[AIR] PM2.5: " + pm25 + " ug/m3\n");

	if (!firstIgnored) {
		Serial.println("[AIR] Ignoring first read");
		firstIgnored = true;
		return false;
	}

	Serial.print("[MQTT] Sending measurement...\n");

	mqttClient.beginMessage("sensors/air/0/temp");
	mqttClient.print(temp);
	mqttClient.endMessage();

	mqttClient.beginMessage("sensors/air/0/pm25");
	mqttClient.print(pm25);
	mqttClient.endMessage();

	return true;
}

bool printSample() {
	if (!dsm501.update()) {
		return false;
	}

	String concentration = String(dsm501.getConcentration());
	Serial.print(concentration + "\n");

	return true;
}

void loop() {
	mqttClient.poll();

	if (sendSample()) {
		printSample();
		failCount = 0;
	} else {
		failCount++;
	}


	if (failCount > MAX_FAILS) {
		Serial.print("Too many failures, resetting Arduino...\n");
		resetFunc();
	}

	delay(100);
}

