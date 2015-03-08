/*
MQTT Wifi Arduino

- connects to an MQTT server
- publishes Status messages and temperature from sensor at predefined interval
- subscribes to the command subtopic in order to publish temperature value on demand
- topic structure:
	Temperature/Messages
			   /Alarms
			   /Statuses
*/

// TO DO
// Provare a cambiare Wifi.cpp http://stackoverflow.com/questions/20339952/mosquitto-socket-read-error-arduino-client
//	per last will che non va


#include <SPI.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <OneWire/OneWire.h>

#include "mqtt_wifi_arduino.h"

// Callback Function: handle message arrived
void callback(char* topic, byte* payload, unsigned int length) {
	// Allocate the correct amount of memory for the payload copy
	char* cmd = (char*)malloc(length) + 1;
	// Copy the payload to the new buffer
	memcpy(cmd, payload, length);
	cmd[length] = 0;
	DEBUG_PRINT(F("---Message Received:"));
	DEBUG_PRINT(cmd);
	DEBUG_PRINT(F("---Message End"));

	//check and execute command
	if (!strcmp(cmd, "getTemp")) {
		if (publishTemp()) {
			DEBUG_PRINT(F("Published"));
			ledOn(Green_led);
		}
		else {
			DEBUG_PRINT(F("Error in publishing"));
			ledOn(Red_led);
		}

	}
	else {
		if (mqttPublish(baseTopic, statusTopic, "Unrecognized Command")) {
			DEBUG_PRINT(F("Published"));
			ledOn(Yellow_led);
		}
		else {
			DEBUG_PRINT(F("Error in publishing"));
			ledOn(Red_led);
		}

	}

	// Free the memory
	free(cmd);
}

WiFiClient WFclient;
PubSubClient client(server, mqttPort, callback, WFclient);
OneWire ds(DS18S20_Pin);

// Turn a single LED on or off
void inline ledOn(int led) {
	digitalWrite(Red_led, LOW);
	digitalWrite(Yellow_led, LOW);
	digitalWrite(Green_led, LOW);
	digitalWrite(led, HIGH);
}

// subscribe inline function
boolean inline mqttSubscribe(const char *base, const char *sub) {
	strcpy(printbuf, base);
	strcat(printbuf, sub);
	return (client.subscribe(printbuf));
}

// publish inline function
boolean inline mqttPublish(const char *base, const char *sub, char *payload) {
	strcpy(printbuf, base);
	strcat(printbuf, sub);
	return (client.publish(printbuf, payload));
}

void setup()
{
// use serial only in debug mode
#ifdef DEBUG
	Serial.begin(9600);
#endif

	// init LEDs
	pinMode(Red_led, OUTPUT);
	pinMode(Yellow_led, OUTPUT);
	pinMode(Green_led, OUTPUT);
	digitalWrite(Red_led, LOW);
	digitalWrite(Yellow_led, LOW);
	digitalWrite(Green_led, LOW);

	// check for the presence of the shield:
	if (WiFi.status() == WL_NO_SHIELD) {
		DEBUG_PRINT(F("WiFi shield not present"));
		// don't continue:
		ledOn(Red_led);
		while (true);
	}
	mqttClientId[0] = 0;
	if (doConnect(mqttClientId))	{
		if (mqttSubscribe(baseTopic, cmdTopic)) {
			DEBUG_PRINT(F("Subscribed to cmd"));
			ledOn(Green_led);
		}
		else {
			DEBUG_PRINT(F("Error in subscribing"));
			ledOn(Red_led);
		}

		if (mqttPublish(baseTopic, statusTopic, "Now Online")) {
			DEBUG_PRINT(F("Published"));
			ledOn(Green_led);
		}
		else {
			DEBUG_PRINT(F("Error in publishing"));
			ledOn(Red_led);
		}

		if (publishTemp()) {
			DEBUG_PRINT(F("Published"));
			ledOn(Green_led);
		}
		else {
			DEBUG_PRINT(F("Error in publishing"));
			ledOn(Red_led);
		}

	}
	else {
		DEBUG_PRINT(F("Connection KO!"));
		ledOn(Red_led);
	}
}

void loop()
{
	unsigned long curTime = millis();

	if (!client.connected()) {
		doConnect(mqttClientId);
		if (mqttSubscribe(baseTopic, cmdTopic)) {
			DEBUG_PRINT(F("Subscribed to cmd"));
			ledOn(Green_led);
		}
		else {
			DEBUG_PRINT(F("Error in subscribing"));
			ledOn(Red_led);
		}
	}

	if (curTime - prevTime > interval) {
		// read temperature from sensor and publish it to message topic
		if (publishTemp()) {
			DEBUG_PRINT(F("Published"));
			ledOn(Green_led);
			prevTime = curTime;
		}
		else {
			DEBUG_PRINT(F("Error in publishing"));
			ledOn(Red_led);
		}
	}
	delay(5000);
	client.loop();
}

void printWifiStatus() {
	// print the SSID of the network you're attached to:
	Serial.print("SSID: ");
	Serial.println(WiFi.SSID());

	// print your WiFi shield's IP address:
	IPAddress ip = WiFi.localIP();
	Serial.print("IP Address: ");
	Serial.println(ip);

	// print the received signal strength:
	long rssi = WiFi.RSSI();
	Serial.print("signal strength (RSSI):");
	Serial.print(rssi);
	Serial.println(" dBm");
}

// use the formatted MAC address as ClientId
void MQTTgetClientId(char *ClientId)
{
	uint8_t mac[WL_MAC_ADDR_LENGTH];
	int j = 0;

	WiFi.macAddress(mac);
	for (int i = WL_MAC_ADDR_LENGTH; i > 0; i--) {
		sprintf(&ClientId[j], "%02X:", mac[i]);
		j += 3;
	}
	ClientId[j - 1] = 0;
	DEBUG_PRINT(ClientId);
}

// connect to wifi network and MQTT server
bool doConnect(char *ClientId)
{
	bool retval = false;

	ledOn(Yellow_led);
	// attempt to connect to Wifi network:
	while (status != WL_CONNECTED) {
		DEBUG_PRINT(F("Connecting to network"));
		// Connect to WPA/WPA2 network. Change this line if using open or WEP network:    
		status = WiFi.begin((char*)ssid, (char*)wifiPass);
		// wait 5 seconds for connection:
		delay(5000);
	}
	DEBUG_PRINT(F("Connected to wifi"));
#ifdef DEBUG
	printWifiStatus();
#endif

	// if still empty initialize ClientId
	if (ClientId[0] == 0) {
		MQTTgetClientId(ClientId);
	}

	// try to connect to MQTT server
	while (!client.connected())	{
		DEBUG_PRINT(F("Connecting to MQTT server"));

		strcpy(printbuf, baseTopic);
		strcat(printbuf, alarmTopic); // will topic
		//retval = client.connect(mqttClientId, (char*)mqttUser, (char*)mqttPass, printbuf, 0, true, "Client lost connection");
		retval = client.connect(mqttClientId, (char*)mqttUser, (char*)mqttPass);
		delay(5000);
	}
	DEBUG_PRINT(F("Connected to MQTT server"));
	ledOn(Green_led);
	return retval;
}

// read temperature from sensor and publish it to message topic
boolean publishTemp() {
	float temperature = getTemp();
	String tString = "Temp.: " + String(dtostrf(temperature, 1, 2, tempbuf)) + " C";
	tString.toCharArray(tempbuf, 20);
	DEBUG_PRINT(tempbuf);
	return mqttPublish(baseTopic, messageTopic, tempbuf);
}

//return the temperature from one DS18S20 in DEG Celsius
float getTemp() {

	byte data[12];
	byte addr[8];

	if (!ds.search(addr)) {
		//no more sensors on chain, reset search
		ds.reset_search();
		ledOn(Red_led);
		return -1000;
	}

	if (OneWire::crc8(addr, 7) != addr[7]) {
		DEBUG_PRINT(F("CRC is not valid!"));
		ledOn(Red_led);
		return -1000;
	}

	if (addr[0] != 0x10 && addr[0] != 0x28) {
		DEBUG_PRINT(F("Device is not recognized"));
		ledOn(Red_led);
		return -1000;
	}

	ds.reset();
	ds.select(addr);
	ds.write(0x44, 1); // start conversion, with parasite power on at the end

	byte present = ds.reset();
	ds.select(addr);
	ds.write(0xBE); // Read Scratchpad


	for (int i = 0; i < 9; i++) { // we need 9 bytes
		data[i] = ds.read();
	}

	ds.reset_search();

	byte MSB = data[1];
	byte LSB = data[0];

	float tempRead = ((MSB << 8) | LSB); //using two's compliment
	float TemperatureSum = tempRead / 16;

	return TemperatureSum;

}