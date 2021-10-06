#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

ESP8266WiFiMulti WiFiMulti;

#define ANALOG_PIN		A0

// should sample for 1 second at 200 Hz x 20
#define SAMPLE_DELAY        5
#define NUM_SAMPLES         200
#define NUM_AVERAGES        20
#define POWER_THRESHOLD     100

void setup() {

	Serial.begin(115200);
	// Serial.setDebugOutput(true);

	Serial.println();
	Serial.println();
	Serial.println();
	Serial.println("==============");
	Serial.println("[INFO] Boot up");

	delay(1000);

	WiFi.mode(WIFI_STA);
	WiFiMulti.addAP("Protospace", "yycmakers");

}

int getSample() {
	Serial.println("[DATA] Sampling power data for 20 seconds...");
	Serial.print("[DATA] Samples: ");

	int numAbove = 0;

	for (int j = 0; j < NUM_AVERAGES; j++) {
		long total = 0;

		for (int i = 0; i < NUM_SAMPLES; i++) {
			int sample = analogRead(ANALOG_PIN);
			total += sample;
			delay(SAMPLE_DELAY);
		}

		int average = total / NUM_SAMPLES;
		Serial.printf("%d, ", average);

		if (average > POWER_THRESHOLD) {
			numAbove++;
		}
	}

	Serial.println();
	Serial.printf("[DATA] Num samples above threshold: %d\n", numAbove);
	return numAbove;
}

bool sendData(String url, int data) {
	Serial.print("[HTTP] Sending data to ");
	Serial.println(url);

	if (WiFiMulti.run() != WL_CONNECTED) {
		Serial.println("[WIFI] Not connected yet.");
		return false;
	}
	
	std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
	client->setInsecure();
	HTTPClient https;
	Serial.println("[HTTP] Begin...");

	if (!https.begin(*client, url)) {
		Serial.println("[HTTP] Unable to connect.");
		https.end();
		return false;
	}

	https.addHeader("Content-Type", "application/x-www-form-urlencoded");
	int httpCode = https.POST("device=TROTECS300&data=" + String(data));

	if (httpCode <= 0) {
		Serial.printf("[HTTP] POST... failed, error: %s\n", https.errorToString(httpCode).c_str());
		https.end();
		return false;
	}

	Serial.printf("[HTTP] POST... code: %d\n", httpCode);

	// file found at server
	if (httpCode != HTTP_CODE_OK) {
		Serial.println("[HTTP] Bad response code.");
		https.end();
		return false;
	}

	String response = https.getString();
	Serial.println("[HTTP] Response:");
	Serial.println(response);

	https.end();
	return true;
}

void loop() {
	int data = getSample();

	//sendData("https://api.my.protospace.ca/stats/usage/", data);
	sendData("https://api.spaceport.dns.t0.vc/stats/usage/", data);
}
