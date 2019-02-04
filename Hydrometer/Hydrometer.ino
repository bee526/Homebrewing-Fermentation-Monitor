/*  Hydrometer Tilt Sensor
    ESP-WROOM-02
    MMA8452

   ChangeLog
   01/22/2019
   
   Inital setup,
   OTA function, with manual override loop in setup, redirects wifi connection to home wifi
   UDP communication client for connecting and sending data to the host ESP
   Accelerometer integration using MMA8452 library

   01/27/2019 - REV 1
   Adding specific messaging

   01/28/2019 - REV 2
   Commented CODE and Reorginized

   02/03/2019 - Rev 3
   Updated battery function for reading battery voltage, May not be sensitive enough at the high end, might need additional resistance.
   Fixed logic on test macros
   Connected Accelerometer


   TO DO
   Add message error checking, Call and Responce
   Determine data collection for Accelerometer
   
*/



#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <SparkFun_MMA8452Q.h>


/**************************** DEFINITIONS AND VARIABLES ********************************/
// DEFINE FIRMWARE
#define FIRMWARE_VERSION 3


// TEST ENABLES
#define TEST_ENABLE_TEST_DATA false
#define TEST_DISABLE_SLEEP    false


// DEFINE ESP TO ESP WIFI CREDENTIALS
#define udp_name              "hydrometer"
#define udp_wifi_ssid         "Hydrometer"
#define udp_wifi_password     "RottenPotato"
#define udp_port              2000

IPAddress ServerIP(192, 168, 4, 1);
IPAddress ClientIP(192, 168, 4, 2);


// DEFINE OVER THE AIR (OTA) CREDENTIALS
#define ota_wifi_ssid         "Tomato"
#define ota_wifi_password     "RottenPotato"
#define ota_name              "hydrometer"
#define ota_password          "123"
#define ota_port              8266


// DEFINE PIN DEFINITIONS
#define wakeup_pin            D0  // Pin set to wake up the ESP after sleeping
#define ota_pin_low           D3  // Drive bin low for easy OTA pin handleing
#define ota_pin_input         D4  // Input pin, set to low to start OTA during boot
#define battery_voltage       A0
// SCL D1                         // Default arduino preprocessed 
// SDA D2                         // Default arduino preprocessed



// DEFINE VARIABLE DEFINITIONS
const int BUFFER_SIZE = 512;      // Buffer used to for packaging message JSON
int sensorSleepInterval = 30;     // Default sleep interval in seconds
int sensorSampleNumber = 0;       // Default number of samples to obtain


// DEFINE OBJECT CLASS
WiFiUDP udp;                      // UDP object
MMA8452Q accel;                   // Accelerometer object



/********************************** START SETUP ****************************************/
void setup() {
  // Connect to computer serial port
  Serial.begin(115200);
  Serial.println();
  Serial.println();
  Serial.println("Starting Hydrometer");


  Serial.println("Configure Pins");
  // Set ota_pin_low to ground for easier jumper
  pinMode(ota_pin_low, OUTPUT);
  digitalWrite(ota_pin_low, LOW);

  // Set ota_pin_input to input with internal pullup
  pinMode(ota_pin_input, INPUT_PULLUP);

  // Connect D0 to RST to wake up
  pinMode(wakeup_pin, WAKEUP_PULLUP);

  // Set Battery voltage input pin
  pinMode(A0, INPUT);


  // Check if OTA jumper is connected
  Serial.println("Check if OTA in is SET");
  while (!digitalRead(ota_pin_input)) {
    Serial.println("Starting OTA WiFi");
    setupWifi(ota_name, ota_wifi_ssid, ota_wifi_password);
    if (WiFi.isConnected()) {
      setupArduinoOTA();
      otaUpdateLoop();  // Stays in loop until OTA pin is removed
      Serial.println("OTA not received, Disconnecting OTA WiFi");
      WiFi.disconnect();
    }
  }


  // Connect to ESP UDP Host WiFi
  Serial.println("Starting WIFI setup");
  setupWifi(udp_name, udp_wifi_ssid, udp_wifi_password);


  // Start UDP
  Serial.println("Starting UDP");
  udp.begin(udp_port);
  Serial.print("SSID: ");
  Serial.println(udp_wifi_ssid);
  Serial.println(udp_wifi_ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Local port: ");
  Serial.println(udp.localPort());


  // Send initial connection message
  sendMessage(packageMessageConnected());

  // Wait for responce, up to 20 sec
  Serial.println("Wait for reply");
  for (int i = 0; i <= 10; i++) {
    if (checkMessageReceived()) {
      Serial.println();
      Serial.println("Reply was Received");
      break;
    }
    delay(500);
    Serial.print(".");
  }

  if (!TEST_ENABLE_TEST_DATA) {
  // Start Sensor
  Serial.println("Starting Sensor");
  accel.init(); 


  // Wait for accelerometer data
  Serial.println("Wait for accelerometer data");
  while (!accel.available()) {}  // Need to update to non blocking in case of error


  // Call accel.read() to update accel variables
  Serial.println("Update accelerometer variables");
  accel.read();
}

  // Send Data
  Serial.println("Send Values");
  Serial.println();
  sendMessage(packageMessageValues());


  // Put to Sleep
  if (!TEST_DISABLE_SLEEP) {
    // Send disconnected message
    sendMessage(packageMessageDisconnected());

    Serial.println(("Put ESP to Sleep for " + String(sensorSleepInterval) + " seconds").c_str());
    delay(1000);
    ESP.deepSleep(sensorSleepInterval * 1e6);
    delay(100);
  }
}

/********************************** START MAIN LOOP***************************************/
void loop() {}


/******************************** WIFI / OTA  FUNCTIONS **************************************/

/*
 *  Sets up and connects to wifi
 */
void setupWifi(String device_name, String wifi_ssid, String wifi_password) {
  WiFi.mode(WIFI_STA); // Ensure that it is configured for STATION MODE
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  WiFi.mode(WIFI_STA);
  WiFi.hostname(device_name);
  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());

  int count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    count++;
    delay(500);
    Serial.print(".");
    if (count >= 60) {
      Serial.println("WiFi Failed to Connect");
      return;
    }
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}



/*
 * Sets up arduino Over The Air, for updating the arduino code via wifi
 */
void setupArduinoOTA() {
  // Port defaults to 8266
  ArduinoOTA.setPort(ota_port);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(ota_name);

  // No authentication by default
  ArduinoOTA.setPassword(ota_password);

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}



/*
 * Loop to that constantly calls OTA handler to check for incoming updates
 */
void otaUpdateLoop() {
  Serial.println("Entering OTA Update Loop");
  Serial.println("Waiting for OTA Update");

  int count = 1;
  while (!digitalRead(ota_pin_input)) {
    ArduinoOTA.handle();
    delay(1000);
    count++;
    if (count == 60) {
      Serial.println("Waiting for OTA Update");
      count = 1;
    }
  }
  Serial.println("OTA Update Not Received");
  Serial.println("Exiting OTA Update Loop");
}






/******************************* DATA FUNCTIONS *************************************/

/*
 * Battery voltage or life function,
 * ESP analog pin reads from 0 to 1v, the onboard voltage divider (GND -100k- analog input -220k- A0) boost it to from 0 to 3.3v
 * An additional 100k is provided between the battery + terminal to A0, to allow from 0 to 4.2v
 * Full charge of the battery is 4.2v
 * Dead battery is ~3.3v
 */
float getBattery() {
  return analogRead(A0) * (4.2/1023);
}


/*
 * Converts the dBm to a range between 0 and 100%
 */
int getWifiQuality() {
  int dbm = WiFi.RSSI();
  if (dbm <= -100) {
    return 0;
  } else if (dbm >= -50) {
    return 100;
  } else {
    return 2 * (dbm + 100);
  }
}



/******************************* MESSAGING FUNCTIONS *************************************/

/*
 * Handler that checks and handles avialible incoming UDP data
 */
bool checkMessageReceived() {
  int packetSize = udp.parsePacket();
  if (packetSize) {
    Serial.printf("Received %d bytes from %s, port %d\n", packetSize, udp.remoteIP().toString().c_str(), udp.remotePort());
    char packetBuffer[packetSize];      // Declare packet buffer
    udp.read(packetBuffer, packetSize); // Read the packet into the buffer
    delay(20);
    Serial.println(packetBuffer);
    unpackageMessage(packetBuffer);
    return true;
  }
  return false;
}


/*
 * Unpacks the JSON message and update the state variables
 */
void unpackageMessage(char* payload) {
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

  Serial.println("Unpacking Message:");
  JsonObject& root = jsonBuffer.parseObject(payload);

  // Test if parsing succeeds.
  if (root.success()) {

    // Unpack message name
    String message = root["message"];
    sensorSleepInterval = root["sleep"];
    sensorSampleNumber = root["samples"];
    Serial.printf("[%s] Sleep: %d, Samples: %d \n\n", message.c_str(), sensorSleepInterval, sensorSampleNumber);

  } else {
    Serial.println("Parsing JSON failed!");
    Serial.println();
    return;
  }
}


/*
 * Package the local data for Output Connected message
 */
char* packageMessageConnected() {
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

  JsonObject& root = jsonBuffer.createObject();

  char* message = "Connected";

  root["message"] = message;
  root["battery"] = getBattery();
  root["signal"] = getWifiQuality();

  char buffer[root.measureLength() + 1];
  root.printTo(buffer, sizeof(buffer));
  return buffer;

}

/*
 * Package the local data for Output Disconnected message
 */
char* packageMessageDisconnected() {
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

  JsonObject& root = jsonBuffer.createObject();

  char* message = "Disconnected";

  root["message"] = message;
  root["battery"] = getBattery();
  root["signal"] = getWifiQuality();

  char buffer[root.measureLength() + 1];
  root.printTo(buffer, sizeof(buffer));
  return buffer;

}


/*
 * Package the local data for Output Values message
 */
char* packageMessageValues() {
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

  JsonObject& root = jsonBuffer.createObject();

  char* message = "Sensor_Values";

  if (TEST_ENABLE_TEST_DATA) {
    // Test placebo data
    root["message"] = message;
    root["accel.x"] = millis();
    root["accel.y"] = millis();
    root["accel.z"] = millis();
    root["accel.cx"] = millis();
    root["accel.cy"] = millis();
    root["accel.cz"] = millis();

  } else {
    // Real sensor Data
    root["message"] = message;
    root["accel.x"] = accel.x;
    root["accel.y"] = accel.y;
    root["accel.z"] = accel.z;
    root["accel.cx"] = accel.cx;
    root["accel.cy"] = accel.cy;
    root["accel.cz"] = accel.cz;
  }
  
  char buffer[root.measureLength() + 1];
  root.printTo(buffer, sizeof(buffer));

  return buffer;
}


/*
 * Sends the packaged message to Serail & UDP 
 */
void sendMessage(char* package) {

  // Send over serial
  Serial.println("Sending " + String(strlen(package)) + " bytes ");
  Serial.println(package);
  Serial.println();
  delay(500);

  // Send over UDP
  udp.beginPacket(ServerIP, udp_port);
  udp.write(package);
  udp.endPacket();
}
