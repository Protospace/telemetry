#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

ESP8266WiFiMulti WiFiMulti;

void setup() {

	Serial.begin(115200);
	// Serial.setDebugOutput(true);

	Serial.println();
	Serial.println();
	Serial.println();

	for (uint8_t t = 4; t > 0; t--) {
		Serial.printf("[SETUP] WAIT %d...\n", t);
		Serial.flush();
		delay(1000);
	}

	WiFi.mode(WIFI_STA);
	WiFiMulti.addAP("Protospace", "yycmakers");
}

void loop() {
	// wait for WiFi connection
	if ((WiFiMulti.run() == WL_CONNECTED)) {

		std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);

		client->setInsecure();

		HTTPClient https;

		Serial.print("[HTTPS] begin...\n");
		if (https.begin(*client, "https://api.my.protospace.ca/stats/bay_108_temp/")) {
			Serial.print("[HTTPS] POST...\n");
			// start connection and send HTTP header
			https.addHeader("Content-Type", "application/x-www-form-urlencoded");
			int httpCode = https.POST("data=1234");

			// httpCode will be negative on error
			if (httpCode > 0) {
				// HTTP header has been send and Server response header has been handled
				Serial.printf("[HTTPS] POST... code: %d\n", httpCode);

				// file found at server
				if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
					String payload = https.getString();
					Serial.println(payload);
				}
			} else {
				Serial.printf("[HTTPS] POST... failed, error: %s\n", https.errorToString(httpCode).c_str());
			}

			https.end();
		} else {
			Serial.printf("[HTTPS] Unable to connect\n");
		}
	}

	Serial.println("Wait 60s before next round...");
	delay(60000);
}
