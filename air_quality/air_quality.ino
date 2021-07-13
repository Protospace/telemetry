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
#define MAX_FAILS       5

WiFiClientSecure wc;
MqttClient mqttClient(wc);
SHT3X sht30(0x45);
DSM501 dsm501;

void (* resetFunc) (void) = 0;

const char broker[] = "webhost.protospace.ca";
int        port     = 8883;

#define PM25_TOPIC   "test/air/1/pm25"
#define TEMP_TOPIC   "test/air/1/temp"
#define LOG_TOPIC    "test/air/1/log"

bool firstIgnored = false;
long failCount = 0;

void sendMqtt(String topic, String msg) {
  mqttClient.beginMessage(topic);
  mqttClient.print(msg);
  mqttClient.endMessage();
}

void setup() {

	Serial.begin(115200);
	Serial.setDebugOutput(true);

	Serial.println();
	Serial.println();
	Serial.println();

	delay(1000);

	WiFi.mode(WIFI_STA);
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
	Serial.print("Connecting to WiFi");
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}
	Serial.println("Connected to WiFi");

	// Synchronize time using NTP. This is necessary to verify that
	// the TLS certificates offered by the server are currently valid.
	Serial.print("Setting time using NTP");
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

  X509List cert(lets_encrypt_ca);
  wc.setTrustAnchors(&cert);
  // wc.setInsecure();  // disables all SSL checks. don't use in production
  
	mqttClient.setUsernamePassword(MQTT_USERNAME, MQTT_PASSWORD);

	Serial.printf("[MQTT] Connecting to broker...\n");

	if (!mqttClient.connect(broker, port)) {
		Serial.print("[MQTT] Connection failed! Error code = ");
		Serial.println(mqttClient.connectError());
		Serial.printf("Resetting Arduino...\n");
		resetFunc();
	}

  sendMqtt(LOG_TOPIC, "Boot up");

	// Initialize DSM501:
	//           PM1.0 pin     PM2.5 pin     sampling duration in seconds
	dsm501.begin(DSM501_PM1_0, DSM501_PM2_5, SAMPLE_TIME);

	// Wait 120s for DSM501 to warm up
	Serial.println("Wait 120s for DSM501 to warm up");
	for (int i = 1; i <= 120; i++)
	{
		mqttClient.poll();
		delay(1000); // 1s
		Serial.print(i);
		Serial.println(" s (wait 120s for DSM501 to warm up)");
	}

	Serial.println("DSM501 is ready!");
	Serial.println();

  sendMqtt(LOG_TOPIC, "DSM501 ready");
}

int sendSample() {
	if (!dsm501.update()) {
		return -1;
	}

	if (sht30.get()) {
		Serial.printf("[TEMP] Unable to get temperature\n");
		return 0;
	}

	if (WiFi.status() != WL_CONNECTED) {
		Serial.printf("[WIFI] Not connected\n");
		Serial.printf("[WIFI] Reconnecting...\n");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
		return 0;
	}

	Serial.printf("[MQTT] Checking connection to broker...\n");

	if (!mqttClient.connected()) {
		Serial.print("[MQTT] Not connected! Error code = ");
		Serial.println(mqttClient.connectError());
		Serial.printf("[MQTT] Reconnecting...\n");
		mqttClient.connect(broker, port); 
		return 0;
	}

	String temp = String(sht30.cTemp);
	Serial.print("[TEMP] Temperature: " + temp + " C\n");
	String pm25 = String(dsm501.getConcentration());
	Serial.print("[AIR] PM2.5: " + pm25 + " ug/m3\n");

	if (!firstIgnored) {
		Serial.println("[AIR] Ignoring first read");
    sendMqtt(LOG_TOPIC, "Ignore first");
		firstIgnored = true;
		return 0;
	}

	if (pm25 == "0.00") {
		Serial.println("[AIR] Bad zero reading");
    sendMqtt(LOG_TOPIC, "Bad zero reading");
		return 0;
	}

	Serial.print("[MQTT] Sending measurement...\n");
  sendMqtt(TEMP_TOPIC, temp);
  sendMqtt(PM25_TOPIC, pm25);

	Serial.print("Done.\n");
	return 1;
}

void loop() {
	mqttClient.poll();

	int res = sendSample();
	if (res == 1) {
		failCount = 0;
		Serial.println("Reset fail count.");
	} else if (res == 0) {
		failCount++;
		Serial.print("Increased fail count to: ");
		Serial.println(failCount);
	}

	if (failCount >= MAX_FAILS) {
		Serial.print("Too many failures, resetting Arduino...\n");
    sendMqtt(LOG_TOPIC, "Failure reset");
		resetFunc();
	}
}
