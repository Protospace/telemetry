/*
 *  Spaceport Push Notification 1.5 (esp8266)
 *  Board: LOLIN(WEMOS) D1 R2 & mini
 *
 *  Sample messages:
 *  {'data': 'Connected'}
 *  {'data': 'Trouble status on'}
 *  {'data': 'Exit delay in progress: Partition 1'}
 *  {'data': 'Disarmed: Partition 1'}
 *  {'data': 'Armed away: Partition 1'}
 *  {'data': 'Alarm: Partition 1'}
 *  {'data': 'Alarm: Partition 2'}
 *  {'data': 'Zone alarm restored: 1'}
 *  {'data': 'Zone alarm restored: 3'}
 *  {'data': 'Disarmed: Partition 1'}
 *  {'data': 'Disarmed: Partition 2'}
 *
 *  Processes the security system status and demonstrates how to send a push notification when the status has changed.
 *  This example sends notifications via Spaceport: https://api.my.protospace.ca
 *
 *  Usage:
 *    1. Set the WiFi SSID and password in the sketch.
 *    2. Generate an access token in Spaceport's secrets.py
 *    3. Copy the access token to spaceportToken.
 *    4. Upload the sketch.
 *
 *  Release notes:
 *    1.5 - Update HTTPS root certificate for api.my.protospace.ca
 *          Added DSC Classic series support
 *    1.4 - Add HTTPS certificate validation, add customizable message prefix
 *    1.3 - Updated esp8266 wiring diagram for 33k/10k resistors
 *    1.2 - Check if WiFi disconnects and wait to send updates until reconnection
 *          Add appendPartition() to simplify sketch
 *          esp8266 Arduino Core version check for BearSSL
 *    1.1 - Set authentication method for BearSSL in esp8266 Arduino Core 2.5.0+
 *          Added notifications - Keybus connected, armed status, zone alarm status
 *    1.0 - Initial release
 *
 *  Wiring:
 *      DSC Aux(+) --- 5v voltage regulator --- esp8266 development board 5v pin (NodeMCU, Wemos)
 *
 *      DSC Aux(-) --- esp8266 Ground
 *
 *                                         +--- dscClockPin  // Default: D1, GPIO 5
 *      DSC Yellow --- 33k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *                                         +--- dscReadPin  // Default: D2, GPIO 4
 *      DSC Green ---- 33k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *      Classic series only, PGM configured for PC-16 output:
 *      DSC PGM ---+-- 1k ohm resistor --- DSC Aux(+)
 *                 |
 *                 |                       +--- dscPC16Pin   // Default: D7, GPIO 13
 *                 +-- 33k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *  Issues and (especially) pull requests are welcome:
 *  https://github.com/taligentx/dscKeybusInterface
 *
 *  This example code is in the public domain.
 */

// DSC Classic series: uncomment for PC1500/PC1550 support (requires PC16-OUT configuration per README.md)
//#define dscClassicSeries

#include <ESP8266WiFi.h>
#include <dscKeybusInterface.h>

#include "secrets.h"


// Configures the Keybus interface with the specified pins.
#define dscClockPin D1  // GPIO 5
#define dscReadPin  D2  // GPIO 4
#define dscPC16Pin  D7  // DSC Classic Series only, GPIO 13

// HTTPS root certificate for api.my.protospace.ca: ISRG Root X1, expires Mon, 04 Jun 2035 11:04:38 GMT
const char spaceportCertificateRoot[] = R"=EOF=(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)=EOF=";

// Initialize components
#ifndef dscClassicSeries
dscKeybusInterface dsc(dscClockPin, dscReadPin);
#else
dscClassicInterface dsc(dscClockPin, dscReadPin, dscPC16Pin);
#endif
X509List spaceportCert(spaceportCertificateRoot);
WiFiClientSecure ipClient;
bool wifiConnected = true;


void setup() {
	Serial.begin(115200);
	delay(1000);
	Serial.println();
	Serial.println();

	Serial.print(F("WiFi...."));
	WiFi.mode(WIFI_STA);
	WiFi.begin(wifiSSID, wifiPassword);
	ipClient.setTrustAnchors(&spaceportCert);
	//ipClient.setInsecure();
	while (WiFi.status() != WL_CONNECTED) {
		Serial.print(".");
		delay(500);
	}
	Serial.print(F("connected: "));
	Serial.println(WiFi.localIP());

	Serial.print(F("NTP time...."));
	configTime(0, 0, "pool.ntp.org");
	time_t now = time(nullptr);
	while (now < 24 * 3600)
	{
		Serial.print(".");
		delay(2000);
		now = time(nullptr);
	}
	Serial.println(F("synchronized."));

	// Sends a message on startup to verify connectivity
	Serial.print(F("Spaceport...."));
	if (sendMessage("Initializing")) Serial.println(F("connected."));
	else Serial.println(F("connection error."));

	// Starts the Keybus interface
	dsc.begin();
	Serial.println(F("DSC Keybus Interface is online."));
}


void loop() {

	// Updates status if WiFi drops and reconnects
	if (!wifiConnected && WiFi.status() == WL_CONNECTED) {
		Serial.println("WiFi reconnected");
		wifiConnected = true;
		dsc.pauseStatus = false;
		dsc.statusChanged = true;
	}
	else if (WiFi.status() != WL_CONNECTED && wifiConnected) {
		Serial.println("WiFi disconnected");
		wifiConnected = false;
		dsc.pauseStatus = true;
	}

	dsc.loop();

	if (dsc.statusChanged) {      // Checks if the security system status has changed
		dsc.statusChanged = false;  // Reset the status tracking flag

		// If the Keybus data buffer is exceeded, the sketch is too busy to process all Keybus commands.  Call
		// loop() more often, or increase dscBufferSize in the library: src/dscKeybus.h or src/dscClassic.h
		if (dsc.bufferOverflow) {
			Serial.println(F("Keybus buffer overflow"));
			dsc.bufferOverflow = false;
		}

		// Checks if the interface is connected to the Keybus
		if (dsc.keybusChanged) {
			dsc.keybusChanged = false;  // Resets the Keybus data status flag
			if (dsc.keybusConnected) sendMessage("Connected");
			else sendMessage("Disconnected");
		}

		// Checks status per partition
		for (byte partition = 0; partition < dscPartitions; partition++) {

			// Skips processing if the partition is disabled or in installer programming
			if (dsc.disabled[partition]) continue;

			// Checks armed status
			if (dsc.armedChanged[partition]) {
				if (dsc.armed[partition]) {
					char messageContent[30];

					if (dsc.armedAway[partition] && dsc.noEntryDelay[partition]) strcpy(messageContent, "Armed night away: Partition ");
					else if (dsc.armedAway[partition]) strcpy(messageContent, "Armed away: Partition ");
					else if (dsc.armedStay[partition] && dsc.noEntryDelay[partition]) strcpy(messageContent, "Armed night stay: Partition ");
					else if (dsc.armedStay[partition]) strcpy(messageContent, "Armed stay: Partition ");

					appendPartition(partition, messageContent);  // Appends the message with the partition number
					sendMessage(messageContent);
				}
				else {
					char messageContent[22] = "Disarmed: Partition ";
					appendPartition(partition, messageContent);  // Appends the message with the partition number
					sendMessage(messageContent);
				}
			}

			// Checks exit delay status
			if (dsc.exitDelayChanged[partition]) {
				dsc.exitDelayChanged[partition] = false;  // Resets the exit delay status flag

				if (dsc.exitDelay[partition]) {
					char messageContent[36] = "Exit delay in progress: Partition ";
					appendPartition(partition, messageContent);  // Appends the message with the partition number
					sendMessage(messageContent);
				}
				else if (!dsc.exitDelay[partition] && !dsc.armed[partition]) {
					char messageContent[22] = "Disarmed: Partition ";
					appendPartition(partition, messageContent);  // Appends the message with the partition number
					sendMessage(messageContent);
				}
			}

			// Checks alarm triggered status
			if (dsc.alarmChanged[partition]) {
				dsc.alarmChanged[partition] = false;  // Resets the partition alarm status flag

				if (dsc.alarm[partition]) {
					char messageContent[19] = "Alarm: Partition ";
					appendPartition(partition, messageContent);  // Appends the message with the partition number
					sendMessage(messageContent);
				}
				else if (!dsc.armedChanged[partition]) {
					char messageContent[22] = "Disarmed: Partition ";
					appendPartition(partition, messageContent);  // Appends the message with the partition number
					sendMessage(messageContent);
				}
			}
			dsc.armedChanged[partition] = false;  // Resets the partition armed status flag

			// Checks fire alarm status
			if (dsc.fireChanged[partition]) {
				dsc.fireChanged[partition] = false;  // Resets the fire status flag

				if (dsc.fire[partition]) {
					char messageContent[24] = "Fire alarm: Partition ";
					appendPartition(partition, messageContent);  // Appends the message with the partition number
					sendMessage(messageContent);
				}
				else {
					char messageContent[33] = "Fire alarm restored: Partition ";
					appendPartition(partition, messageContent);  // Appends the message with the partition number
					sendMessage(messageContent);
				}
			}
		}

		// Checks for zones in alarm
		// Zone alarm status is stored in the alarmZones[] and alarmZonesChanged[] arrays using 1 bit per zone, up to 64 zones
		//   alarmZones[0] and alarmZonesChanged[0]: Bit 0 = Zone 1 ... Bit 7 = Zone 8
		//   alarmZones[1] and alarmZonesChanged[1]: Bit 0 = Zone 9 ... Bit 7 = Zone 16
		//   ...
		//   alarmZones[7] and alarmZonesChanged[7]: Bit 0 = Zone 57 ... Bit 7 = Zone 64
		if (dsc.alarmZonesStatusChanged) {
			dsc.alarmZonesStatusChanged = false;                           // Resets the alarm zones status flag
			for (byte zoneGroup = 0; zoneGroup < dscZones; zoneGroup++) {
				for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
					if (bitRead(dsc.alarmZonesChanged[zoneGroup], zoneBit)) {  // Checks an individual alarm zone status flag
						bitWrite(dsc.alarmZonesChanged[zoneGroup], zoneBit, 0);  // Resets the individual alarm zone status flag
						if (bitRead(dsc.alarmZones[zoneGroup], zoneBit)) {       // Zone alarm
							char messageContent[15] = "Zone alarm: ";
							char zoneNumber[3];
							itoa((zoneBit + 1 + (zoneGroup * 8)), zoneNumber, 10); // Determines the zone number
							strcat(messageContent, zoneNumber);
							sendMessage(messageContent);
						}
						else {
							char messageContent[24] = "Zone alarm restored: ";
							char zoneNumber[3];
							itoa((zoneBit + 1 + (zoneGroup * 8)), zoneNumber, 10); // Determines the zone number
							strcat(messageContent, zoneNumber);
							sendMessage(messageContent);
						}
					}
				}
			}
		}

		// Checks trouble status
		if (dsc.troubleChanged) {
			dsc.troubleChanged = false;  // Resets the trouble status flag
			if (dsc.trouble) sendMessage("Trouble status on");
			else sendMessage("Trouble status restored");
		}

		// Checks for AC power status
		if (dsc.powerChanged) {
			dsc.powerChanged = false;  // Resets the battery trouble status flag
			if (dsc.powerTrouble) sendMessage("AC power trouble");
			else sendMessage("AC power restored");
		}

		// Checks panel battery status
		if (dsc.batteryChanged) {
			dsc.batteryChanged = false;  // Resets the battery trouble status flag
			if (dsc.batteryTrouble) sendMessage("Panel battery trouble");
			else sendMessage("Panel battery restored");
		}

		// Checks for keypad fire alarm status
		if (dsc.keypadFireAlarm) {
			dsc.keypadFireAlarm = false;  // Resets the keypad fire alarm status flag
			sendMessage("Keypad Fire alarm");
		}

		// Checks for keypad aux auxiliary alarm status
		if (dsc.keypadAuxAlarm) {
			dsc.keypadAuxAlarm = false;  // Resets the keypad auxiliary alarm status flag
			sendMessage("Keypad Aux alarm");
		}

		// Checks for keypad panic alarm status
		if (dsc.keypadPanicAlarm) {
			dsc.keypadPanicAlarm = false;  // Resets the keypad panic alarm status flag
			sendMessage("Keypad Panic alarm");
		}
	}
}


bool sendMessage(const char* messageContent) {

	// Connects and sends the message as a Spaceport note-type push
	if (!ipClient.connect("api.my.protospace.ca", 443)) return false;
	ipClient.println(F("POST /stats/alarm/ HTTP/1.1"));
	ipClient.println(F("Host: api.my.protospace.ca"));
	ipClient.println(F("User-Agent: ESP8266 Alarm Tracker"));
	ipClient.println(F("Accept: */*"));
	ipClient.println(F("Content-Type: application/json"));
	ipClient.print(F("Content-Length: "));
	ipClient.println(strlen(messagePrefix) + strlen(messageContent) + 11); // 11 chars for json junk
	ipClient.print(F("Authorization: Bearer "));
	ipClient.println(spaceportToken);
	ipClient.println();
	ipClient.print(F("{\"data\":\""));
	ipClient.print(messagePrefix);
	ipClient.print(messageContent);
	ipClient.print(F("\"}"));

	// Waits for a response
	unsigned long previousMillis = millis();
	while (!ipClient.available()) {
		dsc.loop();
		if (millis() - previousMillis > 5000) {
			Serial.println(F("Connection timed out waiting for a response."));
			ipClient.stop();
			return false;
		}
	}

	// Reads the response until the first space - the next characters will be the HTTP status code
	while (ipClient.available()) {
		if (ipClient.read() == ' ') break;
	}

	// Checks the first character of the HTTP status code - the message was sent successfully if the status code
	// begins with "2"
	char statusCode = ipClient.read();

	// Successful, reads the remaining response to clear the client buffer
	if (statusCode == '2') {
		while (ipClient.available()) ipClient.read();
		ipClient.stop();
		return true;
	}

	// Unsuccessful, prints the response to serial to help debug
	else {
		Serial.println(F("Push notification error, response:"));
		Serial.print(statusCode);
		while (ipClient.available()) Serial.print((char)ipClient.read());
		Serial.println();
		ipClient.stop();
		return false;
	}
}


void appendPartition(byte sourceNumber, char* messageContent) {
	char partitionNumber[2];
	itoa(sourceNumber + 1, partitionNumber, 10);
	strcat(messageContent, partitionNumber);
}
