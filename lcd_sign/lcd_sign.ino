#include <Arduino.h>

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <LiquidCrystal.h>

ESP8266WiFiMulti WiFiMulti;

#include <LiquidCrystal_74HC595.h>
//Wemos pins
#define SHCP D7 // Serial Clock SRCLK 74HC595 pin 11
#define STCP D6 // Regiser Clock RCLK 74HC595 pin 12
#define DS D5   // Serial Data SER    74HC595 pin 14

//74HC595 pins new prototype 2021-12-04
#define RS 7
#define E 5
#define d4 4
#define d5 3
#define d6 2
#define d7 1

LiquidCrystal_74HC595 lcd(DS, SHCP, STCP, RS, E, d4, d5, d6, d7);

void setup() {

	Serial.begin(115200);
	// Serial.setDebugOutput(true);

	lcd.begin(16, 2);
	lcd.noAutoscroll();

	Serial.println();
	Serial.println();
	Serial.println();
	Serial.println("==============");
	Serial.println("[INFO] Boot up");

	delay(1000);

	WiFi.mode(WIFI_STA);
	WiFiMulti.addAP("Protospace", "yycmakers");

}


bool getMessage(String url) {
	Serial.print("[HTTP] Getting stats from ");
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

	int httpCode = https.GET();

	if (httpCode <= 0) {
		Serial.printf("[HTTP] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
		https.end();
		return false;
	}

	Serial.printf("[HTTP] GET... code: %d\n", httpCode);

	// file found at server
	if (httpCode != HTTP_CODE_OK) {
		Serial.println("[HTTP] Bad response code.");
		https.end();
		return false;
	}

	String payload = https.getString();
	Serial.println("[HTTP] Response:");
	Serial.println(payload);

	const size_t capacity = JSON_ARRAY_SIZE(0) + 3*JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(13) + 490;
	DynamicJsonBuffer jsonBuffer(capacity);
	JsonObject& root = jsonBuffer.parseObject(payload);

	String sign = root["sign"];

	Serial.println("[DATA] Sign:");
	Serial.println(sign);

	lcd.clear();
	lcd.print(sign.substring(0, 16));
	lcd.setCursor(0, 1);
	lcd.print(sign.substring(16, 32));

	https.end();
	return true;
}

void loop() {
	getMessage("https://api.my.protospace.ca/stats/");
	//getMessage("https://api.spaceport.dns.t0.vc/stats/");

	delay(5000);
}
