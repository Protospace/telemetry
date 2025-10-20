// Board: Wemos D1 Mini

#include <ArduinoMqttClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ElegantOTA.h>         // v2.2.9
#include "secrets.h"

WiFiClient wc;
MqttClient mqttClient(wc);
ESP8266WebServer server(80);

const char broker[] = "172.17.17.181";
int        port     = 1883;
const char topic[]  = "train/control/speed";
#define QOS_2 2

void (* resetFunc) (void) = 0;

void setup() {
	pinMode(D1, OUTPUT);
	pinMode(D2, OUTPUT);
	digitalWrite(D1, LOW);
	digitalWrite(D2, LOW);

	Serial.begin(115200);
	Serial.println("");
	Serial.println("");
	Serial.println("===== BOOT UP =====");

	static int error_count = 0;

	WiFi.hostname("prototrain1");
	WiFi.mode(WIFI_STA);
	WiFi.begin(SECRET_SSID, SECRET_PASS);

	Serial.print("[WIFI] Attempting to connect to wifi");
	while (WiFi.status() != WL_CONNECTED) {
		error_count += 1;
		delay(500);
		Serial.print(".");

		if (error_count > 20) {
			Serial.println("");
			Serial.println("[WIFI] Unable to connect, resetting...");
			resetFunc();
		}
	}

	Serial.println("");
	Serial.println("[WIFI] Connected to the network");
	Serial.println();

	Serial.print("[WIFI] Connected to: ");
	Serial.print(SECRET_SSID);
	Serial.print(", IP address: ");
	Serial.println(WiFi.localIP());

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

	server.on("/", []() {
		server.send(200, "text/html", "<i>SEE YOU STEEL COWBOY...</i>");
	});

	ElegantOTA.begin(&server);    // Start ElegantOTA
	server.begin();
	Serial.println("[HTTP] server started");

	mqttClient.setId("prototrain");

	Serial.print("[MQTT] Attempting to connect to the MQTT broker: ");
	Serial.println(broker);

	if (!mqttClient.connect(broker, port)) {
		Serial.print("MQTT connection failed! Error code = ");
		Serial.println(mqttClient.connectError());
		Serial.println("Reseting...");
		resetFunc();
	}

	Serial.println("[MQTT] Connected to the MQTT broker.");

	mqttClient.onMessage(onMqttMessage);

	Serial.print("[MQTT] Subscribing to topic: ");
	Serial.println(topic);

	mqttClient.subscribe(topic, QOS_2);

	Serial.println("[MQTT] Waiting for messages.");
}

void loop() {
	static int error_count = 0;

	if (WiFi.status() != WL_CONNECTED) {
		error_count += 1;
		Serial.println("[WIFI] Lost connection. Reconnecting...");
		WiFi.begin(SECRET_SSID, SECRET_PASS);
		delay(5000);
	} else if (!mqttClient.connected()) {
		error_count += 1;
		Serial.print("[MQTT] Not connected! Error code = ");
		Serial.println(mqttClient.connectError());
		Serial.println("[MQTT] Reconnecting...");
		mqttClient.connect(broker, port); 
		delay(5000);

		Serial.print("[MQTT] Subscribing to topic: ");
		Serial.println(topic);
		mqttClient.subscribe(topic, QOS_2);
	} else {
		error_count = 0;
		mqttClient.poll();
	}

	if (error_count > 10) {
		Serial.println("Over 10 errors, resetting...");
		resetFunc();
	}

	server.handleClient();

}

void onMqttMessage(int messageSize) {
	String topic = mqttClient.messageTopic();
	Serial.print("[MQTT] Received a message with topic '");
	Serial.print(topic);
	Serial.print("', length ");
	Serial.print(messageSize);
	Serial.println(" bytes:");

	String message = "";

	while (mqttClient.available()) {
		message += (char)mqttClient.read();
	}

	Serial.println(message);

	if (topic != "train/control/speed") {
		Serial.println("[MQTT] Invalid topic, returning.");
		return;
	}

	int64_t num = message.toInt();
	processControlCommand(num);
}

void processControlCommand(int64_t num) {
	static int64_t prev_num = 0;

	Serial.print("[CHOO] Setting power: ");
	Serial.println(num);
	if (num > 0) {
		analogWrite(D1, num);
		analogWrite(D2, 0);
	} else {
		analogWrite(D1, 0);
		analogWrite(D2, -num);
	}
}
