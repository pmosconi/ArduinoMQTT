// Update these with values suitable for your network.
uint8_t server[] = { 54, 77, 160, 162 }; // MQTT server IP address
#define mqttPort 1883

const char ssid[] = "Pallina1";			// network SSID (name) 
const char wifiPass[] = "Cicciob8";		// network password (use for WPA, or use as key for WEP)
const char mqttUser[] = "testmqtt1";	// MQTT user
const char mqttPass[] = "Password123!";	// MQTT password
// MQTT topic and subtopics
const char baseTopic[] = "Temperature";		
const char messageTopic[] = "/Messages";
const char alarmTopic[] = "/Alarms";
const char statusTopic[] = "/Statuses";
const char cmdTopic[] = "/Command";

// publishing interval: 300000 ms = 5 min
#define interval 300000

unsigned long prevTime = 0;

char mqttClientId[18];					// ClientId

int status = WL_IDLE_STATUS;
char printbuf[100];
char tempbuf[20];

// WiFi Shield Reserved pins
//  4 SS for SD Card
//  7 Handshake
// 10 SS for WiFi
// 11 SPI
// 12 SPI
// 13 SPI

// OneWire DS18S20 Signal pin on digital 3
#define DS18S20_Pin 3

// LED pins
#define Red_led 6
#define Yellow_led 8
#define Green_led 9

// debugger definition and macro
// uncomment the following line to debug
// #define DEBUG

#ifdef DEBUG
#define DEBUG_PRINT(x)  Serial.println(x)
#else
#define DEBUG_PRINT(x)
#endif