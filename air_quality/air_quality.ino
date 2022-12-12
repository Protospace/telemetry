// Protospace Air Quality sensor V2
// Board: Adafruit QT Py ESP32-S2

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoMqttClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "Adafruit_PM25AQI.h"
#include "Adafruit_SGP30.h"
#include "secrets.h"
#include "lets_encrypt_ca.h"

#define SAMPLE_TIME     60 // seconds
#define SENDING_DURATION_MS 60*1000
#define MAX_FAILS       5

WiFiClientSecure wc;
MqttClient mqttClient(wc);

Adafruit_PM25AQI aqi = Adafruit_PM25AQI();

Adafruit_SGP30 sgp;

void (* resetFunc) (void) = 0;

const char broker[] = "webhost.protospace.ca";
int        port     = 8883;

#define DATA_TOPIC   "test/air/4/data"
#define LOG_TOPIC    "test/air/4/log"
#define MQTT_ID      "air4"

long failCount = 0;
int initial_ignored_count = 0;

void sendMqtt(String topic, String msg) {
	Serial.println(msg);
	mqttClient.beginMessage(topic);
	mqttClient.print(msg);
	mqttClient.endMessage();
}

// from https://arduinojson.org/v6/how-to/configure-the-serialization-of-floats
double round2(double value) {
	return (int)(value * 100 + 0.5) / 100.0;
}

void setup() {
	Serial.begin(115200);
	delay(1000);
	//Serial.setDebugOutput(true);

	Serial.println();
	Serial.println();
	Serial.println();
	Serial.println("===== BOOT UP =====");

	delay(1000);

	WiFi.mode(WIFI_STA);
	WiFi.begin(WIFI_SSID, WIFI_PASS);
	Serial.print("[WIFI] Connecting...");
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}
	Serial.println();
	Serial.println("[WIFI] Connected.");

	// Synchronize time using NTP. This is necessary to verify that
	// the TLS certificates offered by the server are currently valid.
	Serial.print("[TIME] Setting time using NTP");
	configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");
	time_t now = time(nullptr);
	while (now < 8 * 3600 * 2) {
		delay(500);
		Serial.print(".");
		now = time(nullptr);
	}
	Serial.println();
	struct tm timeinfo;
	gmtime_r(&now, &timeinfo);
	Serial.print("[TIME] Current time: ");
	Serial.print(asctime(&timeinfo));
	Serial.println(" UTC");

	//X509List cert(lets_encrypt_ca);
	//wc.setTrustAnchors(&cert);
	wc.setInsecure();  // disables all SSL checks. don't use in production

	mqttClient.setUsernamePassword(MQTT_USERNAME, MQTT_PASSWORD);

	Serial.println("[MQTT] Connecting to broker...");

	if (!mqttClient.connect(broker, port)) {
		Serial.print("[MQTT] Connection failed! Error code = ");
		Serial.println(mqttClient.connectError());
		Serial.println("[MQTT] Resetting Arduino...");
		resetFunc();
	}

	sendMqtt(LOG_TOPIC, "[META] Boot up");

	// QT Py ESP32-S2 has two I2C ports, switch to the header
	Wire.setPins(SDA1, SCL1);

	if (aqi.begin_I2C()){
		sendMqtt(LOG_TOPIC, "[META] Started particle sensor");
	} else {
		sendMqtt(LOG_TOPIC, "[META] Error starting particle sensor");
		resetFunc();
	}
	delay(1000);

	if (sgp.begin()){
		sendMqtt(LOG_TOPIC, "[META] Started VOC sensor");
	} else {
		sendMqtt(LOG_TOPIC, "[META] Error starting VOC sensor");
		resetFunc();
	}
	delay(1000);

	if (sgp.serialnumber[2] == 0x2D3E) {
		sgp.setIAQBaseline(0x8EAA, 0x8DB5);
		Serial.println("[META] Set baseline to: 0x8EAA 0x8DB5");
	}
	delay(1000);
}

int sendSample(String data) {
	if (WiFi.status() != WL_CONNECTED) {
		Serial.println("[WIFI] Not connected");
		Serial.println("[WIFI] Reconnecting...");
		WiFi.begin(WIFI_SSID, WIFI_PASS);
		return 0;
	}

	Serial.println("[MQTT] Checking connection to broker...");

	if (!mqttClient.connected()) {
		Serial.print("[MQTT] Not connected! Error code = ");
		Serial.println(mqttClient.connectError());
		Serial.println("[MQTT] Reconnecting...");
		mqttClient.connect(broker, port); 
		return 0;
	}

	if (initial_ignored_count < 2) {
		sendMqtt(LOG_TOPIC, "[DATA] Ignoring first 2 samples");
		initial_ignored_count++;
		return 1;
	}

	sendMqtt(LOG_TOPIC, "[DATA] Sending measurement");
	sendMqtt(DATA_TOPIC, data);

	Serial.println("[MQTT] Done.");
	return 1;
}


void loop() {
	static int num_fails = 0;
	static unsigned long prev_sent_time = millis();

	static int num_dust_samples = 0;
	static float total_p01_std = 0;
	static float total_p25_std = 0;
	static float total_p10_std = 0;
	static float total_p01_env = 0;
	static float total_p25_env = 0;
	static float total_p10_env = 0;
	static float total_003um = 0;
	static float total_005um = 0;
	static float total_010um = 0;
	static float total_025um = 0;
	static float total_050um = 0;
	static float total_100um = 0;

	static int num_voc_samples = 0;
	static float total_tvoc = 0;
	static float total_eco2 = 0;

	mqttClient.poll();

	//  ============= Dust Sensor ==============

	PM25_AQI_Data pm_data;
	if (aqi.read(&pm_data)) {
		total_p01_std += pm_data.pm10_standard;
		total_p25_std += pm_data.pm25_standard;
		total_p10_std += pm_data.pm100_standard;
		total_p01_env += pm_data.pm10_env;
		total_p25_env += pm_data.pm25_env;
		total_p10_env += pm_data.pm100_env;
		total_003um += pm_data.particles_03um;
		total_005um += pm_data.particles_05um;
		total_010um += pm_data.particles_10um;
		total_025um += pm_data.particles_25um;
		total_050um += pm_data.particles_50um;
		total_100um += pm_data.particles_100um;

		num_dust_samples++;
	}


	//  ============= VOC Sensor ===============

	if (sgp.IAQmeasure()) {
		total_tvoc += sgp.TVOC;
		total_eco2 += sgp.eCO2;

		num_voc_samples++;
	}


	//  ============= Send Sample ==============

	unsigned long now_ms = millis();
	unsigned long elapsed = now_ms - prev_sent_time;
	if (elapsed >= SENDING_DURATION_MS) {  // overflow safe
		Serial.println("[MQTT] Sending data...");

		prev_sent_time = now_ms;

		String data = "";
		const size_t capacity = JSON_OBJECT_SIZE(15);
		StaticJsonDocument<capacity> root;

		root["id"] = MQTT_ID;

		// dust
		root["p01_std"] = round2(total_p01_std / num_dust_samples);
		root["p25_std"] = round2(total_p25_std / num_dust_samples);
		root["p10_std"] = round2(total_p10_std / num_dust_samples);
		root["p01_env"] = round2(total_p01_env / num_dust_samples);
		root["p25_env"] = round2(total_p25_env / num_dust_samples);
		root["p10_env"] = round2(total_p10_env / num_dust_samples);
		root["003um"] = round2(total_003um / num_dust_samples);
		root["005um"] = round2(total_005um / num_dust_samples);
		root["010um"] = round2(total_010um / num_dust_samples);
		root["025um"] = round2(total_025um / num_dust_samples);
		root["050um"] = round2(total_050um / num_dust_samples);
		root["100um"] = round2(total_100um / num_dust_samples);
		num_dust_samples = 0;
		total_p01_std = 0;
		total_p25_std = 0;
		total_p10_std = 0;
		total_p01_env = 0;
		total_p25_env = 0;
		total_p10_env = 0;
		total_003um = 0;
		total_005um = 0;
		total_010um = 0;
		total_025um = 0;
		total_050um = 0;
		total_100um = 0;

		// voc
		root["tvoc"] = total_tvoc / num_voc_samples;
		root["eco2"] = total_eco2 / num_voc_samples;
		num_voc_samples = 0;
		total_tvoc = 0;
		total_eco2 = 0;


		serializeJson(root, data);

		if (sendSample(data) == 1) {
			num_fails = 0;
			sendMqtt(LOG_TOPIC, "[MQTT] Good send, reset fail count.");
		} else {
			num_fails++;
			sendMqtt(LOG_TOPIC, "[MQTT] Bad send, increased fail count.");
		}
	}

	if (num_fails >= MAX_FAILS) {
		sendMqtt(LOG_TOPIC, "[MQTT] Too many failures, resetting...");
		resetFunc();
	}

	delay(200);
}
