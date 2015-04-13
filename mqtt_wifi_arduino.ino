/*
MQTT Wifi Arduino

- connects to an MQTT server
- publishes Status messages and temperature from sensor at predefined interval
- subscribes to the command subtopic in order to publish temperature value on demand
- topic structure:
	Temperature/Messages
			   /Alarms
			   /Statuses
			   /Command
- output structure:
	{"Key":value}
	where Key is Message | Error | Temperature
- input structure:
	{"Command":"getValue"}
	{"Command":"setFreq","Hz":val}
	{"Command":"setParam","fSample":num}
	{"Command":"setParam","Interval":sec}
	*/

// TO DO
// Provare a cambiare Wifi.cpp http://stackoverflow.com/questions/20339952/mosquitto-socket-read-error-arduino-client
//	per last will che non va
//
// HO FINITO LA MEMORIA... interval viene sovrascritto


#include <SPI.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <OneWire/OneWire.h>
#include <ArduinoJson/ArduinoJson.h>
#include <FreqMeasure/FreqMeasure.h>


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

	//Parse and validate JSON
	// StaticJsonBuffer<100> jsonBuffer;
	DynamicJsonBuffer jsonBuffer;

	JsonObject& json = jsonBuffer.parseObject(cmd);
	if (!json.success()) {
		if (mqttPublish(baseTopic, statusTopic, "Error", "JSON Error")) {
			DEBUG_PRINT(F("Published"));
			ledOn(Yellow_led);
		}
		else {
			DEBUG_PRINT(F("Error in publishing"));
			ledOn(Red_led);
		}
		free(cmd);
		return;
	}

	//check and execute command getValue
	if (!strcmp_P(json[cmdKey], PSTR("getValue"))) {
		if (publishTemp() && publishReadFreq(lastReadFreq) && publishInterval()) {
			DEBUG_PRINT(F("Published"));
			ledOn(Green_led);
		}
		else {
			DEBUG_PRINT(F("Error in publishing"));
			ledOn(Red_led);
		}
		free(cmd);
		return;
	}

	//check and execute command setFreq
	if (!strcmp_P(json[cmdKey], PSTR("setFreq")) && json[HzKey] > 0) {
		if (publishSetFreq(json[HzKey])) {
			DEBUG_PRINT(F("Published"));
			ledOn(Green_led);
		}
		else {
			DEBUG_PRINT(F("Error in publishing"));
			ledOn(Red_led);
		}
		free(cmd);
		return;
	}

	//check and execute command setParam
	if (!strcmp_P(json[cmdKey], PSTR("setParam")) && json[freqSampleKey] > 0) {
		if (publishSetFreqSample(json[freqSampleKey])) {
			DEBUG_PRINT(F("Published"));
			ledOn(Green_led);
		}
		else {
			DEBUG_PRINT(F("Error in publishing"));
			ledOn(Red_led);
		}
		free(cmd);
		return;
	}

	//check and execute command setParam
	if (!strcmp_P(json[cmdKey], PSTR("setParam")) && json[intervalKey] > 0) {
		if (publishSetInterval(json[intervalKey])) {
			DEBUG_PRINT(F("Published"));
			ledOn(Green_led);
		}
		else {
			DEBUG_PRINT(F("Error in publishing"));
			ledOn(Red_led);
		}
		free(cmd);
		return;
	}

	// else: unrocognized command
	if (mqttPublish(baseTopic, statusTopic, "Error", "Invalid Command")) {
		DEBUG_PRINT(F("Published"));
		ledOn(Yellow_led);
	}
	else {
		DEBUG_PRINT(F("Error in publishing"));
		ledOn(Red_led);
	}
	free(cmd);
	return;
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
	strcpy_P(printbuf, base);
	strcat_P(printbuf, sub);
	return (client.subscribe(printbuf));
}

// publish inline function
boolean inline mqttPublish(const char *base, const char *sub, const char *key, const char *payload) {
	strcpy_P(printbuf, base);
	strcat_P(printbuf, sub);
	StaticJsonBuffer<100> jsonBuffer;
	JsonObject& json = jsonBuffer.createObject();
	json[key] = payload;
	json.printTo(jtempbuf, sizeof(jtempbuf));
	DEBUG_PRINT(printbuf);
	return (client.publish(printbuf, jtempbuf));
}

boolean inline mqttPublish(const char *base, const char *sub, const char *key, float payload) {
	strcpy_P(printbuf, base);
	strcat_P(printbuf, sub);
	StaticJsonBuffer<100> jsonBuffer;
	JsonObject& json = jsonBuffer.createObject();
	json[key].set(payload,2);
	json.printTo(jtempbuf, sizeof(jtempbuf));
	DEBUG_PRINT(printbuf);
	return (client.publish(printbuf, jtempbuf));
}

// read temperature from sensor and publish it to message topic
boolean inline publishTemp() {
	return mqttPublish(baseTopic, messageTopic, "Temp C", getTemp());
}

// publish message interval value to message topic
boolean inline publishInterval() {
	return mqttPublish(baseTopic, messageTopic, "Msg interval sec", interval / 1000);
}

// set wave output frequency and publish it to message topic
boolean inline publishSetFreq(int freq) {
	tone(freqOut_pin, freq);
	return mqttPublish(baseTopic, messageTopic, "Set Freq Hz", freq);
}

// set frequency sampling value and publish it to message topic
boolean inline publishSetFreqSample(int samples) {
	freqMaxCount = samples;
	return mqttPublish(baseTopic, messageTopic, "Freq Samples", samples);
}

// set message interval value and publish it to message topic
boolean inline publishSetInterval(int seconds) {
	interval = seconds * 1000l;
	return mqttPublish(baseTopic, messageTopic, "Msg Interval sec", seconds);
}

// publish to message topic the measured frequency
boolean inline publishReadFreq(float freq) {
	return mqttPublish(baseTopic, messageTopic, "Measured Freq Hz", freq);
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

	// init freq pins
	pinMode(freqIn_pin, INPUT);
	pinMode(freqOut_pin, OUTPUT);

	tone(freqOut_pin, curFreq);
	FreqMeasure.begin();

	// check for the presence of the shield
	delay(5000); // wait 5 seconds for the shield to boot
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

		if (mqttPublish(baseTopic, statusTopic, "Message", "Now Online")) {
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
		// publish measured frequency to message topic
		if (publishReadFreq(lastReadFreq)) {
			DEBUG_PRINT(F("Published"));
			ledOn(Green_led);
			prevTime = curTime;
		}
		else {
			DEBUG_PRINT(F("Error in publishing"));
			ledOn(Red_led);
		}

	}
	// read frequency
	float frequency = frequencyRead();
	if (frequency > 0 && abs(frequency - lastReadFreq) > .01 ) {
		lastReadFreq = frequency;
		DEBUG_PRINT(F("Frequency:"));
		DEBUG_PRINT(lastReadFreq);
	}

	delay(1000);
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

// start frequency output on freqOut_pin
//void playFreq(int freq) {
//	delay(curFreq * 1.3);
//	curFreq = freq;
//	tone(freqOut_pin, freq);
//
//}

// frequency measure
float frequencyRead() {
	float frequency = -1;

	if (FreqMeasure.available()) {
		// average several reading together
		freqSum += FreqMeasure.read();
		if (freqCount++ > freqMaxCount) {
			frequency = FreqMeasure.countToFrequency(freqSum / freqCount);
			freqSum = 0;
			freqCount = 0;
		}
	}
	return frequency;
}