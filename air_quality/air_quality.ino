// Protospace Air Quality sensor V2
// Board: Adafruit QT Py ESP32-S2

#include <WiFi.h>
#include <ArduinoJson.h>  // v6.21.4
#include <ArduinoMqttClient.h>  // v0.1.6
#include "Adafruit_PM25AQI.h"
#include "Adafruit_SGP30.h"
#include "secrets.h"

#define MAX_FAILS       10
#define SENDING_DURATION_MS 10*1000

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

Adafruit_SGP30 sgp;  // Gas sensor

Adafruit_PM25AQI aqi = Adafruit_PM25AQI();  // PM sensor

const char broker[] = "172.17.17.181";
int        port     = 1883;

int initial_ignored_count = 0;

void sendMqtt(String topic, String msg) {
	Serial.println(msg);

	if (mqttClient.connected()) {
		mqttClient.beginMessage(topic);
		mqttClient.print(msg);
		mqttClient.endMessage();
	}
}

// from https://arduinojson.org/v6/how-to/configure-the-serialization-of-floats
double round2(double value) {
	return (int)(value * 100 + 0.5) / 100.0;
}

void setup() {
	int status;

	Serial.begin(115200);

	Serial.println();
	Serial.println();
	Serial.println();
	Serial.println("===== BOOT UP =====");

	delay(1000);

	WiFi.mode(WIFI_STA);
	WiFi.begin(SECRET_SSID, SECRET_PASS);
	Serial.print("[WIFI] Connecting...");
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}
	Serial.println("");
	Serial.println("[WIFI] Connected.");
	delay(500);

	Serial.println("[MQTT] Connecting to broker...");

	mqttClient.setId(MQTT_ID);

	if (!mqttClient.connect(broker, port)) {
		Serial.print("[MQTT] Connection failed! Error code = ");
		Serial.println(mqttClient.connectError());
		Serial.println("Resetting Arduino...");
		abort();
	}

	sendMqtt(LOG_TOPIC, "[META] Boot up");
	delay(500);

	// QT Py ESP32-S2 has two I2C ports, switch to the header
	Wire.setPins(SDA1, SCL1);

	if (aqi.begin_I2C()){
		sendMqtt(LOG_TOPIC, "[META] Started particle sensor");
	} else {
		sendMqtt(LOG_TOPIC, "[META] Error starting particle sensor");
		delay(500);
		abort();
	}
	delay(1000);

	if (sgp.begin() == 1){
		sendMqtt(LOG_TOPIC, "[META] Started VOC sensor");
	} else {
		sendMqtt(LOG_TOPIC, "[META] Error starting VOC sensor");
		delay(500);
		abort();
	}
	delay(1000);

	sendMqtt(LOG_TOPIC, "[META] Ready");
}

int sendSample(String data) {
	if (WiFi.status() != WL_CONNECTED) {
		Serial.println("[WIFI] Not connected");
		Serial.println("[WIFI] Reconnecting...");
		WiFi.begin(SECRET_SSID, SECRET_PASS);
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

	if (initial_ignored_count < 10) {
		sendMqtt(LOG_TOPIC, "[DATA] Ignoring first 10 samples");
		initial_ignored_count++;
		return 1;
	}

	sendMqtt(LOG_TOPIC, "[DATA] Sending measurement");
	sendMqtt(DATA_TOPIC, data);

	Serial.println("[MQTT] Done.");
	return 1;
}

void loop() {
	bool read_failed = false;
	bool send_failed = false;
	static int num_read_fails = 0;
	static int num_send_fails = 0;

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
		Serial.print("[DATA] PM2.5: ");
		Serial.print(pm_data.pm25_standard);
		Serial.print(", PM10: ");
		Serial.println(pm_data.pm10_standard);

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
	} else {
		Serial.println("No PM sensor reading.");
		read_failed = true;
	}

	//  ============= VOC Sensor ===============

	// TODO: add a temperature / humidity sensor and correct for it
	if (sgp.IAQmeasure()) {
		Serial.print("[DATA] TVOC: ");
		Serial.print(sgp.TVOC);
		Serial.print(", eCO2: ");
		Serial.println(sgp.eCO2);

		total_tvoc += sgp.TVOC;
		total_eco2 += sgp.eCO2;
		num_voc_samples++;
	} else {
		Serial.println("No VOC sensor reading.");
		read_failed = true;
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

		// VOC
		root["tvoc"] = round2(total_tvoc / num_voc_samples);
		root["eco2"] = round2(total_eco2 / num_voc_samples);
		num_voc_samples = 0;
		total_tvoc = 0;
		total_eco2 = 0;


		serializeJson(root, data);

		if (sendSample(data) == 1) {
			sendMqtt(LOG_TOPIC, "[MQTT] Good send.");
		} else {
			sendMqtt(LOG_TOPIC, "[MQTT] Bad send.");
			send_failed = true;
		}

		if (num_send_fails >= MAX_FAILS) {
			sendMqtt(LOG_TOPIC, "[META] Too many send failures, resetting...");
			delay(500);
			abort();
		}
	}

	if (read_failed) {
		num_read_fails++;
	} else {
		num_read_fails = 0;
	}

	if (send_failed) {
		num_send_fails++;
	} else {
		num_send_fails = 0;
	}

	if (num_read_fails >= MAX_FAILS) {
		sendMqtt(LOG_TOPIC, "[META] Too many read failures, resetting...");
		delay(500);
		abort();
	}

	delay(450);
}
