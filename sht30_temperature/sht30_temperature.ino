#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <WEMOS_SHT3X.h>

ESP8266WiFiMulti WiFiMulti;
SHT3X sht30(0x45);

void setup() {

	Serial.begin(115200);
	// Serial.setDebugOutput(true);

	Serial.println();
	Serial.println();
	Serial.println();

	delay(1000);

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
			Serial.print("Get temperature data...\n");

			if (sht30.get() == 0) {
				Serial.print("[HTTPS] POST...\n");

				// start connection and send HTTP header
				https.addHeader("Content-Type", "application/x-www-form-urlencoded");
				int httpCode = https.POST("data=" + String(sht30.cTemp));

				// httpCode will be negative on error
				if (httpCode > 0) {
					// HTTP header has been send and Server response header has been handled
					Serial.printf("[HTTPS] POST... code: %d\n", httpCode);

					// file found at server
					if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
						String payload = https.getString();
						Serial.println(payload);
					}

					Serial.println("Sent, sleeping 60s");
					ESP.deepSleep(60 * 1000000);
					delay(100);
				} else {
					Serial.printf("[HTTPS] POST... failed, error: %s\n", https.errorToString(httpCode).c_str());
				}

			} else {
				Serial.printf("Temperature data failed.\n");
			}

			https.end();
		} else {
			Serial.printf("[HTTPS] Unable to connect\n");
		}
	}

	Serial.println("Error, wait 10s before trying again...");
	delay(10000);
}
