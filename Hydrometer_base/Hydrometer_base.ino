/*  Hydrometer Tilt Base Station
    ESP8266 D1 Mini
    2.4 TFT Touchscreen
   ChangeLog
   01/22/2019
   Inital setup,
   OTA function, with manual override loop in setup, redirects wifi connection to home wifi
   UDP communication client for connecting and sending data to the host ESP
   Accelerometer integration using MMA8452 library
   01/27/2019
   Added specific messaging
   Added screen functionality and control
   Added Serial commands and control
   01/28/2019 - REV 2
   Commented CODE and Reorginized
   
   02/25/2019
   Updated screen to new format, need to add original screen function back in for backwards compatibility
   TO DO: Add sd card detection
   TO DO
   Add message error checking, Call and Responce
   Improve touch detection
   Improve on Screen graphics
   Add Fermintation proces tracking and graphing
   Add Calibration steps
   Save data to SD card
   Add HTTP Web interface
   Add AP_STA, connect to internet
*/



#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>

#include <Adafruit_GFX.h>         // Core graphics library
#include <Fonts/FreeSerifItalic24pt7b.h>
#include <Adafruit_ImageReader.h> // Image-reading functions
#include <Adafruit_ILI9341.h>     // Hardware-specific library
#include <XPT2046_Touchscreen.h>
#include <SPI.h>
#include <SD.h>
#include <math.h>



/**************************** DEFINITIONS AND VARIABLES ********************************/
// DEFINE FIRMWARE
#define FIRMWARE_VERSION 2

// DEFINE ESP TO ESP WIFI CREDENTIALS
#define udp_name           "hydrometer"
#define udp_wifi_ssid      "Hydrometer"
#define udp_wifi_password  "RottenPotato"
#define udp_port           2000

IPAddress ServerIP(192, 168, 4, 1);
IPAddress ClientIP(192, 168, 4, 2);


// DEFINE PIN DEFINITIONS
#define SD_CS              D8
#define TFT_DC             D8
#define TFT_CS             D0
#define TOUCH_CS           D3
// MOSI D7
// MISO D6
// SCK D5
ADC_MODE(ADC_VCC);

// DEFINE VARIABLE DEFINITIONS
const int BUFFER_SIZE = 512;     // Buffer used to for packaging message JSON
char* commandState = "Standby";  // Default state (Standby, Fermentation, Calibration)
bool sensorConnected = false;
int sensorLastUpdate = -10000;
int sensorSleepInterval = 30;    // Default sleep interval in seconds
int sensorSampleNumber = 0;      // Default number of samples

// screen variables
bool enableScreenSD = true;
bool touchActive = false;        // Prevent double screen shift
bool refresh = true;             // refresh full screen, (screen changes)
int screenCount = 3;             // Total number of screens
int screen = 0;                  // Inital screen on startup

// sensor variables 
float sensorAccel_x;
float sensorAccel_y;
float sensorAccel_z;
float sensorAccel_cx;
float sensorAccel_cy;
float sensorAccel_cz;
float sensorBattery;
int sensorSignal;

// DEFINE OBJECT CLASS
WiFiUDP udp;
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
XPT2046_Touchscreen ts(TOUCH_CS);
Adafruit_ImageReader reader;

// SG DATA INITIALIZATION VARIABLES
float firstElement = 0;                                   // First reading
float lastElement = 0;                                    // Currenet SG reading
float prevElement = 0;                                    // Previous SG reading
float abv = 0;                                            // Alcohol by Volume variable

// SD CARD
int dataCount = 0;

/********************************** START SETUP ****************************************/
void setup() {
  Serial.begin(115200);

  Serial.println("Initializing screen...");
  tft.begin();

  Serial.println("Initializing touch screen...");
  ts.begin();

  Serial.print("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("failed!");
    enableScreenSD = false;
  }
  Serial.println("Initialization done.");

  // Splash Screen 2 second
  if (enableScreenSD) {
    drawSplashScreenSD(2);
  } else {
    drawSplashScreen(2);
  }

  Serial.println("Start WiFi Access Point");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(String(udp_wifi_ssid).c_str(), String(udp_wifi_password).c_str());

  // Start UDP
  Serial.println("Start UDP");
  udp.begin(udp_port);
  Serial.print("SSID: ");
  Serial.println(udp_wifi_ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.softAPIP());
  Serial.print("Local port: ");
  Serial.println(udp.localPort());

  Serial.println("End Setup");
  Serial.println();
}

/********************************** START MAIN LOOP***************************************/
void loop() {

  // Check for screen touch
  if (enableScreenSD) {
    checkButtonTouchSD();
  } else {
    checkButtonTouch();
  }

  // Check for serial input
  checkSerialReceived();

  // Check for incoming messages
  checkMessageReceived();

  // Check that the sensor has not timed out
  checkSensorState();


  // draw screen
  if (enableScreenSD) {
    // Draw SD Card Screens
    if (screen == 0) {
      drawMainScreenSD();
    } else if (screen == 1) {
      drawSettingsScreenSD();
    }
  
  } else {
    // Draw NON SD Card Screens
    if (screen == 0) {
      drawButtons();
    } else if (screen == 1) {
      drawAbout();
    } else if (screen == 2) {
      drawSensorValues();
    }  
  }

}


/******************************** STATE FUNCTIONS **************************************/
/*
 * Fermentation State
 * Update Variables
 */
void stateStandby() {
  commandState = "Standby";
  sensorSleepInterval = 30;
  sensorSampleNumber = 0;
}

/*
 * Fermentation State
 * Update Variables
 * TODO, enter fermentation steps
 */
void stateFermentation() {
  commandState = "Fermentation";
  sensorSleepInterval = 3600;
  sensorSampleNumber = 10;
}

/*
 * Calibration State
 * Update Variables
 * TODO, enter calibration steps
 */
void stateCalibration() {
  commandState = "Calibration";
  sensorSleepInterval = 5;
  sensorSampleNumber = 10;
}


/*
 * Function to update the sensor state
 * Checks to see if the sensor did not check in when it was expected to wake up
 */
void checkSensorState() {
  if ((millis()/1000) - sensorLastUpdate < sensorSleepInterval + 60){
    sensorConnected = true;
  } else {
    sensorConnected = false;
  }
  if (enableScreenSD) {
    drawMainScreenSensorLastSD();
  }
}




/*
 * Detect which button was pressed
 * NON SD Sreen
 */
void checkButtonTouch() {
  if (ts.touched()) {
    refresh = true;
    if (!touchActive) {
      touchActive = true;
      TS_Point a = ts.getPoint();
      Serial.println(a.x);

      // Screen 0 is divided into 6 buttons, bottom two are not used
      if (screen == 0) {
        if (a.x >= 3250 && a.x <= 3700) {
          Serial.println("Button 1, Standby Button Pressed");
          stateStandby();
        }
        if (a.x >= 2700 && a.x <= 3150) {
          Serial.println("Button 2, Fermentation Button Pressed");
          stateFermentation();
        }
        if (a.x >= 2090 && a.x <= 2580) {
          Serial.println("Button 3, Calibration Button Pressed");
          stateCalibration();
        }
        if (a.x >= 1550 && a.x <= 2010) {
          Serial.println("Button 4, About Button Pressed");
          screen = 1;
        }
        if (a.x >= 960 && a.x <= 1450) {
          Serial.println("Button 5, Data Button Pressed");
          screen = 2;
        }
        if (a.x >= 350 && a.x <= 910) {
          Serial.println("Button 6");
        }

      // Screen 1 only has the bottom button
      } else if (screen == 1) {
        if (a.x >= 350 && a.x <= 910) {
          Serial.println("Button 6");
          screen = 0;
        }

      // Screen 2 only has the bottom button
      } else if (screen == 2) {
        if (a.x >= 0350 && a.x <= 910) {
          Serial.println("Button 6");
          screen = 0;
        }
      }
      tft.fillScreen(ILI9341_BLACK);
      refresh = true;

    } else {
      touchActive = false;
    }
  }
}


/*
 * Detect which button was pressed
 * SD Screen
 */
void checkButtonTouchSD() {
  int screenOld = screen;
  if (ts.touched()) {
    if (!touchActive) {
      touchActive = true;
      TS_Point a = ts.getPoint();

      if (screen == 0) {
        mainScreenTouchSD(a.x, a.y);
      } else
      if (screen == 1) {
        settingsScreenTouchSD(a.x, a.y);
      }

      if (screen != screenOld) {
      tft.fillScreen(ILI9341_BLACK);
      refresh = true;
      }
      
    } else {
      touchActive = false;
    }
  }
}


/*
 * Main Screen Touch Interface,
 * Inputs the x,y touch
 * Defines the area for each button
 */
void mainScreenTouchSD(int x, int y) {
  // Settings Button Touch
  if (x >= 3500 && x <= 3900 && y >= 200 && y <= 900) {
    Serial.println("Main Screen, Settings Button Touched");
    screen = 1;
  }

  // Bottom Button Touch
  if (x >= 200 && x <= 900 && y >= 200 && y <= 4000) {
    // if sensors is connected and not timed out
    Serial.println("Main Screen, Bottom Button Touched");
    Serial.println("Change State");
    Serial.println("Start recording Fermentation");
  }
}


/*
 * Settings Screen Touch Interface,
 * Inputs the x,y touch
 * Defines the area for each button
 */
void settingsScreenTouchSD(int x, int y) {
  // Exit Button Touch (3500-3900,200-900)
  if (x >= 3500 && x <= 3900 && y >= 200 && y <= 900) {
    Serial.println("Settings Screen, Exit Button Touched");
    screen = 0;
  }

  // ROW1COL1 Button Touch (2750-3350,3100-3850)
  if (x >= 2750 && x <= 3350 && y >= 3100 && y <= 3850) {
    Serial.println("Settings Screen, ROW1COL1 Button Touched");
    reader.drawBMP("/SetR.bmp", tft, 190, 60);
  }

  // ROW1COL2 Button Touch (2750-3350,1100-1850)
  if (x >= 2750 && x <= 3350 && y >= 1100 && y <= 1850) {
    Serial.println("Settings Screen, ROW1COL2 Button Touched");
    reader.drawBMP("/SetR.bmp", tft, 190, 60);
  }

  // ROW1COL3 Button Touch (2750-3350,200-1000)
  if (x >= 2750 && x <= 3350 && y >= 200 && y <= 1000) {
    Serial.println("Settings Screen, ROW1COL3 Button Touched");
    reader.drawBMP("/SetG.bmp", tft, 190, 60);
  }

  // ROW2COL1 Button Touch (1950-2550,3100-3850)
  if (x >= 1950 && x <= 2550 && y >= 3100 && y <= 3850) {
    Serial.println("Settings Screen, ROW2COL1 Button Touched");
    reader.drawBMP("/SetR.bmp", tft, 190, 130);
  }

  // ROW2COL2 Button Touch (1950-2550,1100-1850)
  if (x >= 1950 && x <= 2550 && y >= 1100 && y <= 1850) {
    Serial.println("Settings Screen, ROW2COL2 Button Touched");
    reader.drawBMP("/SetR.bmp", tft, 190, 130);
  }

  // ROW2COL3 Button Touch (1950-2550,200-1000)
  if (x >= 1950 && x <= 2550 && y >= 200 && y <= 1000) {
    Serial.println("Settings Screen, ROW2COL3 Button Touched");
    reader.drawBMP("/SetG.bmp", tft, 190, 130);
  }

  // ROW3COL1 Button Touch (1150-1750,3100-3850)
  if (x >= 1150 && x <= 1750 && y >= 3100 && y <= 3850) {
    Serial.println("Settings Screen, ROW3COL1 Button Touched");
    reader.drawBMP("/SetR.bmp", tft, 190, 200);
  }

  // ROW3COL2 Button Touch (1150-1750,1100-1850)
  if (x >= 1150 && x <= 1750 && y >= 1100 && y <= 1850) {
    Serial.println("Settings Screen, ROW3COL2 Button Touched");
    reader.drawBMP("/SetR.bmp", tft, 190, 200);
  }

  // ROW3COL3 Button Touch (1150-1750,200-1000)
  if (x >= 1150 && x <= 1750 && y >= 200 && y <= 1000) {
    Serial.println("Settings Screen, ROW3COL3 Button Touched");
    reader.drawBMP("/SetG.bmp", tft, 190, 200);
  }

  // ROW4COL1 Button Touch (350-950,3100-3850)
  if (x >= 350 && x <= 950 && y >= 3100 && y <= 3850) {
    Serial.println("Settings Screen, ROW4COL1 Button Touched");
    reader.drawBMP("/SetR.bmp", tft, 190, 270);
  }

  // ROW4COL2 Button Touch (350-950,1100-1850)
  if (x >= 350 && x <= 950 && y >= 1100 && y <= 1850) {
    Serial.println("Settings Screen, ROW4COL2 Button Touched");
    reader.drawBMP("/SetR.bmp", tft, 190, 270);
  }

  // ROW4COL3 Button Touch (350-950,200-1000)
  if (x >= 350 && x <= 950 && y >= 200 && y <= 1000) {
    Serial.println("Settings Screen, ROW4COL3 Button Touched");
    reader.drawBMP("/SetG.bmp", tft, 190, 270);
  }

}


/*
 * Detect screen swipe
 * Not currently used
 */
void checkScreenSwipe() {
  if (ts.touched()) {
    if (!touchActive) {
      touchActive = true;
      TS_Point a = ts.getPoint();
      delay(50);
      TS_Point b = ts.getPoint();

      if (a.x - b.x < -20) {
        screen = (screen + 1);
        if (screen >= screenCount) {
          screen = 0;
        }
      }
      if (a.x - b.x > 20) {
        screen = (screen - 1);
        if (screen >= screenCount) {
          screen = screenCount - 1;
        }
      }
      tft.fillScreen(ILI9341_BLACK);

    }
  } else {
    touchActive = false;
  }
}






/****************************SD SCREEN DRAW FUNCTIONS **********************************/

/*
 * Display splash screen 
 * SD Screen
 */
void drawSplashScreenSD(int seconds) {
  Serial.println("Start Screen Splash Screen");
  reader.drawBMP("/splash.BMP", tft, 0, 0);
  delay((seconds * 1000));
  tft.fillScreen(ILI9341_BLACK);
}


/*
 * Display Main Screen
 * Gets the images from sd card using Adafruit_ImageReader objext
 * SD Screen
 */
void drawMainScreenSD() {
  if (refresh) {
    tft.fillScreen(ILI9341_BLACK);
    reader.drawBMP("/Ferm.bmp", tft, 8, 8);
    reader.drawBMP("/Settings.bmp", tft, 200, 5);
    reader.drawBMP("/STATUS.bmp", tft, 10, 52);

    // Status Tag
    reader.drawBMP("/ReadyS.bmp", tft, 105, 45);               //TODO Tag based on status
    reader.drawBMP("/CurSG.bmp", tft, 10, 90);
    reader.drawBMP("/OrigSG.bmp", tft, 10, 190);
    reader.drawBMP("/CurABV.bmp", tft, 10, 210);
    reader.drawBMP("/Update.bmp", tft, 10, 235);
    reader.drawBMP("/BATT100.bmp", tft, 20, 254);
    reader.drawBMP("/SIG100.bmp", tft, 95, 250);
    reader.drawBMP("/SIG100.bmp", tft, 160, 250);              //TODO update for Interval
    reader.drawBMP("/START.bmp", tft, 10, 275);                //TODO Input Value

    drawMainScreenValuesSD();
  }
  refresh = false;

}


/*
 * Display Main Screen Values
 * Updates only the values and important information, minimizes re-draw time
 * SD Screen
 */
void drawMainScreenValuesSD() {
    char strBuff[10];

    tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
    tft.fillRect(10, 105, 225, 80, ILI9341_BLACK);
    tft.setTextSize(2);
    tft.setFont(&FreeSerifItalic24pt7b);
    dtostrf(lastElement,5, 3, strBuff);
    drawStrings(10, 175, strBuff);
    tft.setFont();
    tft.setTextSize(1);



    // Original SG
    dtostrf(prevElement,5, 3, strBuff);
    drawStrings(100, 194, strBuff);

    // Current ABV
    dtostrf(abv,7, 3, strBuff);
    drawStrings(100, 213, strBuff);

    // Sensor Last Update
    tft.fillRect(148, 233, 50, 10, ILI9341_BLACK);
    drawMainScreenSensorLastSD();

    // Sensor Battery
    //reader.drawBMP("/BATT100.bmp", tft, 25, 254);                         // Posible dynamic image
    tft.fillRect(43, 257, 40, 10, ILI9341_BLACK);
    drawStrings(45, 258, String(sensorBattery) + " V");

    // Sensor Signal
    //reader.drawBMP("/SIG100.bmp", tft, 125, 250);                         // Posible dynamic image
    tft.fillRect(118, 257, 35, 10, ILI9341_BLACK);
    drawStrings(120, 258, String(sensorSignal) + " %");

    // Sensor Interval
    tft.fillRect(183, 257, 35, 10, ILI9341_BLACK);
    drawStrings(185, 258, String(sensorSleepInterval) + " SEC");
}


/*
 * Display Main Screen Values
 * Print out the sensor connected update time
 * SD Screen
 */
void drawMainScreenSensorLastSD() {
  if (sensorConnected) {
    // Sensor Last Update
    if (screen == 0) {
      drawStrings(150, 238, String((millis() - (sensorLastUpdate*1000))/1000) + " SEC         ");
    }
  } else {
    if (screen == 0) {
      drawStrings(150, 238, "Not Connected");
      sensorBattery = 0;
      sensorSignal = 0;
    }
  }
}


/*
 * Display Settings Screen 
 * Gets the images from sd card using Adafruit_ImageReader objext
 * SD Screen
 */
void drawSettingsScreenSD() {
  if (refresh) {
    tft.fillScreen(ILI9341_BLACK);

    reader.drawBMP("/SETMENU.bmp", tft, 8, 8);
    reader.drawBMP("/X.bmp", tft, 200, 5);

    drawStrings(10, 50, "Standby Interval (min)");
    reader.drawBMP("/BlueL.bmp", tft, 5, 60);
    tft.setTextSize(4);
    drawStrings(60, 70, "888");                                //TODO Input Value
    tft.setTextSize(1);
    reader.drawBMP("/BlueR.bmp", tft, 135, 60);
    reader.drawBMP("/SetG.bmp", tft, 190, 60);


    drawStrings(10, 120, "Fermentation Interval (min)");
    reader.drawBMP("/BlueL.bmp", tft, 5, 130);
    tft.setTextSize(4);
    drawStrings(60, 140, "888");                                //TODO Input Value
    tft.setTextSize(1);
    reader.drawBMP("/BlueR.bmp", tft, 135, 130);
    reader.drawBMP("/SetG.bmp", tft, 190, 130);


    drawStrings(10, 190, "Calibration Water (1.000)");
    reader.drawBMP("/BlueL.bmp", tft, 5, 200);
    tft.setTextSize(4);
    drawStrings(60, 210, "888");                                //TODO Input Value
    tft.setTextSize(1);
    reader.drawBMP("/BlueR.bmp", tft, 135, 200);
    reader.drawBMP("/SetG.bmp", tft, 190, 200);


    drawStrings(10, 260, "Calibration Solution");
    reader.drawBMP("/BlueL.bmp", tft, 5, 270);
    tft.setTextSize(4);
    drawStrings(60, 280, "888");                                //TODO Input Value
    tft.setTextSize(1);
    reader.drawBMP("/BlueR.bmp", tft, 135, 270);
    reader.drawBMP("/SetG.bmp", tft, 190, 270);
  }
  refresh = false;

  drawSettingsScreenValuesSD();
}



/*
 * Display Main Screen Values
 * Updates only the values and important information, minimizes re-draw time
 * SD Screen
 */
void drawSettingsScreenValuesSD() {
  
}



/*
 * Helper function to draw text at location x,y
 */
void drawStrings(int x, int y, String text) {
  tft.setCursor(x, y);
  tft.println(text);
}


/*
 * Helper function to draw data lines 
 */
void drawLabelValue(int line, String label, String value) {
  const int labelX = 15;
  const int valueX = 150;

  tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
  drawStrings(labelX, 30 + line * 15, label);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  drawStrings(valueX, 30 + line * 15, value);
}


/**************************** NON SD SCREEN DRAW FUNCTIONS **********************************/

/*
 * Display splash screen 
 * NON SD Screen
 */
void drawSplashScreen(int seconds) {
  Serial.println("Start Screen Splash Screen");
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(ILI9341_RED);
  tft.setTextSize(4);
  tft.println();
  tft.println();
  tft.println("Hydrometer");
  tft.println();
  tft.setTextColor(ILI9341_GREEN);
  delay((seconds * 1000));
  tft.fillScreen(ILI9341_BLACK);
}


/*
 * Draws main buttons, color change based on commandState
 */
void drawButtons() {

  if (refresh) {
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLUE);

    tft.drawRect(10, 18, 220, 45, ILI9341_GREEN);   //3250 - 3700
    if (commandState == "Standby") {
      tft.fillRect(12, 20, 216, 41, ILI9341_GREEN);
      tft.setTextColor(ILI9341_BLUE, ILI9341_GREEN);
      drawStrings(15, 40, "Standby");
    } else {
      tft.fillRect(12, 20, 216, 41, ILI9341_BLUE);
      tft.setTextColor(ILI9341_WHITE, ILI9341_BLUE);
      drawStrings(15, 40, "Standby");
    }

    tft.drawRect(10, 18 + 50, 220, 45, ILI9341_GREEN);  //2700 - 3150
    if (commandState == "Fermentation") {
      tft.fillRect(12, 20 + 50, 216, 41, ILI9341_GREEN);
      tft.setTextColor(ILI9341_BLUE, ILI9341_GREEN);
      drawStrings(15, 40 + 50, "Fermentation");
    } else {
      tft.fillRect(12, 20 + 50, 216, 41, ILI9341_BLUE);
      tft.setTextColor(ILI9341_WHITE, ILI9341_BLUE);
      drawStrings(15, 40 + 50, "Fermentation");
    }

    tft.drawRect(10, 18 + 100, 220, 45, ILI9341_GREEN);   //2090 - 2580
    if (commandState == "Calibration") {
      tft.fillRect(12, 20 + 100, 216, 41, ILI9341_GREEN);
      tft.setTextColor(ILI9341_BLUE, ILI9341_GREEN);
      drawStrings(15, 40 + 100, "Calibration");
    } else {
      tft.fillRect(12, 20 + 100, 216, 41, ILI9341_BLUE);
      tft.setTextColor(ILI9341_WHITE, ILI9341_BLUE);
      drawStrings(15, 40 + 100, "Calibration");
    }

    tft.drawRect(10, 18 + 150, 220, 45, ILI9341_GREEN);  //1550 - 2010
    if (false) {
      tft.fillRect(12, 20 + 150, 216, 41, ILI9341_GREEN);
      tft.setTextColor(ILI9341_BLUE, ILI9341_GREEN);
      drawStrings(15, 40 + 150, "About");
    } else {
      tft.fillRect(12, 20 + 150, 216, 41, ILI9341_BLUE);
      tft.setTextColor(ILI9341_WHITE, ILI9341_BLUE);
      drawStrings(15, 40 + 150, "About");
    }

    tft.drawRect(10, 18 + 200, 220, 45, ILI9341_GREEN);  //960 - 1450
    if (false) {
      tft.fillRect(12, 20 + 200, 216, 41, ILI9341_GREEN);
      tft.setTextColor(ILI9341_BLUE, ILI9341_BLUE);
      drawStrings(15, 40 + 200, "Data");
    } else {
      tft.fillRect(12, 20 + 200, 216, 41, ILI9341_BLUE);
      tft.setTextColor(ILI9341_WHITE, ILI9341_BLUE);
      drawStrings(15, 40 + 200, "Data");
    }

    //tft.drawRect(10, 18 + 250, 220, 45, ILI9341_GREEN);  //350 - 910
    //tft.fillRect(12, 20 + 250, 216, 41, ILI9341_BLUE);

    refresh = false;
  }
}


/*
 * Draws Sensor Values and state data
 */
void drawSensorValues() {
  tft.setFont();
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_WHITE);
  drawStrings(12, 2, "Hydrometer Data");

  drawLabelValue(0, "sensorAccel_x:", String(sensorAccel_x));
  drawLabelValue(1, "sensorAccel_y:", String(sensorAccel_y));
  drawLabelValue(2, "sensorAccel_z:", String(sensorAccel_z));
  drawLabelValue(3, "sensorAccel_cx:", String(sensorAccel_cx));
  drawLabelValue(4, "sensorAccel_cy:", String(sensorAccel_cy));
  drawLabelValue(5, "sensorAccel_cz:", String(sensorAccel_cz));

  drawLabelValue(7, "Mode:", String(commandState));
  drawLabelValue(8, "Battery:", String(sensorBattery) + "V");
  drawLabelValue(9, "Signal:", String(sensorSignal) + "%");
  drawLabelValue(10, "SleepInterval:", String(sensorSleepInterval));
  drawLabelValue(11, "Samples:", String(sensorSampleNumber));


  if (refresh) {
    tft.drawRect(10, 18 + 250, 220, 45, ILI9341_GREEN);  //1550 - 2010
    tft.fillRect(12, 20 + 250, 216, 41, ILI9341_BLUE);
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLUE);
    drawStrings(15, 40 + 250, "BACK");
    refresh = false;
  }
}


/*
 * Draws Abount Data, Data common to host ESP
 * Funcion copied from weather station project
 */
void drawAbout() {
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  drawStrings(10, 20, "Hydrometer Base");
  drawLabelValue(3, "Heap Mem:", String(ESP.getFreeHeap() / 1024) + "kb");
  drawLabelValue(4, "Flash Mem:", String(ESP.getFlashChipRealSize() / 1024 / 1024) + "MB");
  drawLabelValue(5, "WiFi Strength:", String(WiFi.RSSI()) + "dB");
  drawLabelValue(6, "Chip ID:", String(ESP.getChipId()));
  drawLabelValue(7, "VCC: ", String(ESP.getVcc() / 1024.0) + "V");
  drawLabelValue(8, "CPU Freq.: ", String(ESP.getCpuFreqMHz()) + "MHz");
  char time_str[15];
  const uint32_t millis_in_day = 1000 * 60 * 60 * 24;
  const uint32_t millis_in_hour = 1000 * 60 * 60;
  const uint32_t millis_in_minute = 1000 * 60;
  uint8_t days = millis() / (millis_in_day);
  uint8_t hours = (millis() - (days * millis_in_day)) / millis_in_hour;
  uint8_t minutes = (millis() - (days * millis_in_day) - (hours * millis_in_hour)) / millis_in_minute;
  sprintf(time_str, "%2dd%2dh%2dm", days, hours, minutes);
  drawLabelValue(9, "Uptime: ", time_str);

  tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
  drawStrings(15, 200, "Last Reset: ");
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setCursor(15, 210);
  tft.print(ESP.getResetInfo());

  if (refresh) {
    tft.drawRect(10, 18 + 250, 220, 45, ILI9341_GREEN);  //1550 - 2010
    tft.fillRect(12, 20 + 250, 216, 41, ILI9341_BLUE);
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLUE);
    drawStrings(15, 40 + 250, "BACK");
    refresh = false;
  }
}


/******************************* SERIAL FUNCTIONS *************************************/

/*
 * Handler for incoming serial commands
 */
void checkSerialReceived() {

  // Control state by serial command
  if (Serial.available()) {
    delay(10); // wait for all serial data
    String serialMessage = Serial.readString();
    Serial.println("Command Recieved: " + serialMessage);

    if (serialMessage == "Standby") {
      stateStandby();
    }
    else if (serialMessage == "Fermentation") {
      stateFermentation();
    }
    else if (serialMessage == "Calibration") {
      stateCalibration();
    }
    else {
      Serial.println("Not a Valid Command");
      Serial.println("Received: " + serialMessage);
    }
  }


  /* Serial to UDP passthrough Move to separate functions
    if (Serial.available()) {
    delay(10); // wait for all serial data
    int serialSize = Serial.available();
    String serialMessage = Serial.readString();
    char serialMessageBuffer[serialMessage.length()];
    serialMessage.toCharArray(serialMessageBuffer, serialMessage.length());
    sendMessage(serialMessageBuffer);
    }
  */

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
    sensorLastUpdate = millis()/1000;

    // Unpack message name
    String message = root["message"];

    // Determin which message was Received
    if (message == "Connected") {
      sensorBattery = root["battery"];
      sensorSignal = root["signal"];
      Serial.printf("[Sensor Connected] Battery: %f, Signal: %d \n\n", sensorBattery, sensorSignal);
      // Send Reply with current state
      sendMessage(commandState, sensorSleepInterval, sensorSampleNumber);
    }

    else if (message == "Sensor_Values") {
      sensorAccel_x = root["accel.x"];
      sensorAccel_y = root["accel.y"];
      sensorAccel_z = root["accel.z"];
      sensorAccel_cx = root["accel.cx"];
      sensorAccel_cy = root["accel.cy"];
      sensorAccel_cz = root["accel.cz"];

      addDataSG();                                                  // Log new data into data.txt on the SD card
      updateDataCount();                                            // update dataCount variable with number of elements in data.txt
      updateABV();                                                  // Update Alcohol by Volume variable
      
      firstElement = seekElement(1);                                // Update original SG reading
      lastElement = seekElement(dataCount);                         // Update current SG reading
      prevElement= seekElement(dataCount - 1);                      // Update previous SG reading
      drawMainScreenValuesSD();
      
      
      Serial.printf("[Update Accel] Accel_x: %f, Accel_y: %f, Accel_z: %f \n", sensorAccel_x, sensorAccel_y, sensorAccel_z);
      Serial.printf("[Update Accel] Accel_cx: %f, Accel_cy: %f, Accel_cz: %f \n\n", sensorAccel_cx, sensorAccel_cy, sensorAccel_cz);
      
      Serial.printf("[Update SG Values] firstElement: %f, lastElement: %f, : %f \n\n", firstElement, lastElement, prevElement);
    }

    else if (message == "Disconnected") {
      long sensorSleeping = root["sleep"];
      sensorBattery = root["battery"];
      sensorSignal = root["signal"];
      Serial.printf("[Sensor Disconnected] Sleeping: %d, Battery: %f, Signal: %d \n\n", sensorSleeping, sensorBattery, sensorSignal);
    }

    else {
      Serial.println("Unknown message received");
      Serial.println(); // Need to send a reply for unknown message
    }

  } else {
    Serial.println("Parsing JSON failed!");
    Serial.println();  // Need to send a reply for Fail message
    return;
  }
}


/*
 * Package the local data for Output Messages
 */
void sendMessage(char* message, int sleep, int samples) {
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

  JsonObject& root = jsonBuffer.createObject();

  root["message"] = message;
  root["sleep"] = sleep;
  root["samples"] = samples;


  // Check size
  char buffer[root.measureLength() + 1];
  root.printTo(buffer, sizeof(buffer));

  // Send over serial
  Serial.println("Sending " + String(sizeof(buffer)) + " bytes ");
  Serial.println(buffer);
  Serial.println();

  // Send over UDP
  udp.beginPacket(ClientIP, udp_port);
  udp.write(buffer, sizeof(buffer));
  udp.endPacket();
}

/******************************* FERMENTATION MONITORING FUNCTIONS *************************************/

// Convert accelerometer data into specific gravity reading
float sgCalc(float x, float y, float z){
  float valRads = atan2(z,sqrt(sq(x) + sq(y))); // atan of y/x returns in radians
  float valDegs = valRads * 57296 / 1000;       // convert to degrees
  float sg = -0.000381*(valDegs*valDegs) + (0.050828*valDegs) - 0.601579; 
  return sg;
}



// Update Alcohol by Volume variable
void updateABV(){ 
  abv = 131.25 * (firstElement - lastElement); // 131.25*(OG - FG)
}




// NEED TO UPDATE
int fermStatus(){
  int flag = 0;
  int dif = 10;
  
  if (abs(lastElement - prevElement) < dif ){
    flag = 1;
  }
  return flag;
}




// Datalogger function: add calculated SG value to file in SD card
void addDataSG(){
  // make a string for assembling the data to log:
  String dataString = "";

  // append data to the string
  dataString += String(sgCalc(sensorAccel_cx, sensorAccel_cy, sensorAccel_cz));

  // open the file
  File dataFile = SD.open("data.txt", FILE_WRITE);
  
  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(dataString);   
    dataCount++; // upon restart this will no longer be accurate (stored in ram, data in flash)
    dataFile.close();
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println("error opening datalog.txt");
  }
}



// Seeks the nth element of the data file on SD card (first element is '1')
float seekElement(int elem){
  File dataFile = SD.open("data.txt");
  float element = 0;  
  int recNum = 0;
  if(dataFile){
    dataFile.seek(0);
    while(dataFile.available()){
      String s = dataFile.readStringUntil('\n');
      recNum++;
      if(recNum == elem){
        element = s.toFloat();  // update the variable
        break;
      }
    }
  }
  else {
    Serial.println("error opening the file (lastElem)");
  }
  dataFile.close();
  return element;
}




// Updates the dataCount variable with total number of elements in data.txt
void updateDataCount(){
  int elems = 0;
  File dataFile = SD.open("data.txt");
  if (dataFile){
    dataFile.seek(0);
    while (dataFile.available()){
      String s = dataFile.readStringUntil('\n');
      elems++;
    }
  }
  else {
    Serial.println("error opening the file (updateDataCount)");
  }
  dataFile.close();
  dataCount = elems; 
}
