#include <Arduino.h>

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <SoftwareSerial.h>

#include "font6x10.h"
#include "fonthelper.h"

ESP8266WiFiMulti WiFiMulti;
SoftwareSerial mySerial(D1, D2);

#define ARRAY_SIZE(array) ((sizeof(array))/(sizeof(array[0])))

#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) (bitvalue ? bitSet(value, bit) : bitClear(value, bit))

int colour = 6; //global variable for colour - was trying the variable to cloud thing

int goodColors[] = {6,7,8};
/*valid colours are:
  6 red pixel
  7 is green
  8 is orange
  9 seems to start interlacing red and green (in the tables below assume X is the first color and - is the other)
  10 red and orange
  11 green and red
  12 green and orange
  13 orange and red
  14  orange and green
 */

int offset = 6;
int updateTime = 30000;

void parseString(String incStr, int txtColour, int txtOffset) //function to extract characters from strings
{
	for (int i=0; i <incStr.length(); i++){
		printChar (incStr.charAt(i), txtColour, txtOffset);
	}
}

int GetBitmapLocation (char letter) { //look up the location of the character in the bitmap file
	int result = 0;
	for (int i = 0; i < ARRAY_SIZE(__apple6x10_index__); i++){
		if (__apple6x10_index__[i] == letter) {
			result=i;
		}
	}
	return result;
} 

void printChar(char letter, int txtColour, int txtOffset) { //function to draw the character to the screen, extracts one vertical line at a time
	int lineBuffer =0;
	int charWidth = 6; //hardcoded for now, to be fixed once I add multiple fonts
	int charHeight = 10;
	int index = GetBitmapLocation(letter);
	for (int x = 1; x <= charWidth; x++) {
		for (int y = 1; y <= charHeight; y++) {
			bitWrite(lineBuffer,y+txtOffset, bitRead(__apple6x10_bitmap__[(y+(index*charHeight))-1],charWidth-x+(8-charWidth)));
		}
		sendData(lineBuffer, txtColour, txtOffset);
	}
}

void fillSign(int fillColour)
{
	for (int i =0; i <240; i++){
		sendData (16777215,fillColour,0);
	}
}

void sendData(int incData, int txtColour, int txtOffset) { //sends a line to the screen
	//Inc data is a 25 bit value starting at the bottom row and being 0 for off and 1 for on 
	byte tmpByt = 0;

	if (txtColour== 19)
	{txtColour = int(random(6,9));}

	mySerial.write(txtColour); //colour first

	for (int block = 0; block <= 4; block++) {
		for (int row =1; row <=5; row++) {
			bitWrite (tmpByt, 5-row, bitRead(incData, (row+(block*5))-1) ); 
			//the sign is divided up into 5 5 led sections but only has 24 pixels
			//each section is treated as its own 5 bit binary, simply convert your 0's and 1's to an int and add 32 for the first row, 64 for the second etc. 
		}
		mySerial.write(byte(tmpByt+(32+block*32)));

		tmpByt = 0;
	}

	mySerial.write(5);
}

uint8_t psLogoBMP[] = {
	_____XXX, XXX_____,
	___XXXXX, XXXXX___,
	__XXXX__, __XXXX__,
	_XXX____, ____XXX_,
	_XX_____, _____XX_,
	_XX_____, _____XX_,
	_XXXXXXX, XXXXXXX_,
	_XXXXXXX, XXXXXXX_,
	_XX_____, ________,
	_XXXXXXX, XXXXXXX_,
	_XXXXXXX, XXXXXXX_,
	________, _____XX_,
	_XXXXXXX, XXXXXXX_,
	_XXXXXXX, XXXXXXX_,
	___XX___, ___XX___,
	___XX___, ___XX___,
};

void psLogo(int logoColour, int curOffset){ //show a ps logo
	int lineBuffer =0;
	int charWidth = 16; 
	int charHeight = 16;

	for (int x = 1; x <= charWidth; x++) {
		for (int y = 1; y <= charHeight; y++) {
			if (x > 8){
				bitWrite(lineBuffer,y+curOffset, bitRead(psLogoBMP[((y-1)*2)+1],8-(x-8)));
			}
			else {
				bitWrite(lineBuffer,y+curOffset, bitRead(psLogoBMP[(y-1)*2],8-x));
			}

		}
		sendData(lineBuffer, logoColour, curOffset);
	}
}

int recieveText(String incStr)
{  
	parseString(" ",colour,offset);
	int rndColour = goodColors[random(0,2)];
	parseString(incStr,rndColour,random(0,14));
	parseString(" ",colour,offset);
	psLogo(6,3);

	return 1;
}

void setup() {
	Serial.begin(115200);
	// Serial.setDebugOutput(true);

	mySerial.begin(9600); //serial communicatin to the sign

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
	int member_count = root["member_count"];

	Serial.println("[DATA] Sign:");
	Serial.println(sign);

	recieveText(sign);

	https.end();
	return true;
}

void loop() {
	getMessage("https://api.my.protospace.ca/stats/");
	//getMessage("https://api.spaceport.dns.t0.vc/stats/");

	delay(5000);
}
