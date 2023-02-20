#include <Arduino.h>

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <LiquidCrystal_I2C.h>

#define PROTOCOIN_SYMBOL 0

byte protocoinSymbol[] = {
	B01110,
	B10101,
	B11111,
	B10100,
	B11111,
	B00101,
	B11111,
	B00100
};

ESP8266WiFiMulti WiFiMulti;
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {

	Serial.begin(115200);
	// Serial.setDebugOutput(true);

	lcd.init();
	lcd.createChar(PROTOCOIN_SYMBOL, protocoinSymbol);
	lcd.backlight();
	lcd.clear();
	lcd.home();
	lcd.print("Booting up...");

	Serial.println();
	Serial.println();
	Serial.println();
	Serial.println("==============");
	Serial.println("[INFO] Boot up");

	delay(1000);

	WiFi.mode(WIFI_STA);
	WiFiMulti.addAP("Protospace", "yycmakers");

}

bool getBalance(String url) {
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

	if (payload == "200") {
		Serial.println("No json, displaying welcome message.");
		lcd.clear();
		lcd.home();
		lcd.print("WAITING FOR");
		lcd.setCursor(0, 1);
		lcd.print("LOGIN");
		return true;
	}
		
	StaticJsonDocument<1024> jsonDoc;
	deserializeJson(jsonDoc, payload);

	String first_name = jsonDoc["first_name"].as<String>();
	float balance = jsonDoc["balance"];

	Serial.print("[DATA] Name: ");
	Serial.print(first_name);
	Serial.print(", balance: ");
	Serial.println(balance);

	lcd.clear();
	lcd.home();
	lcd.print(first_name);
	lcd.setCursor(0, 1);
	lcd.write(PROTOCOIN_SYMBOL);
	lcd.setCursor(1, 1);
	lcd.print(balance);

	https.end();
	return true;
}

void loop() {
	getBalance("https://api.my.protospace.ca/protocoin/printer_balance/");
	//getBalance("https://api.spaceport.dns.t0.vc/protocoin/printer_balance/");

	delay(5000);
}
