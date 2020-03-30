#include <Arduino.h>

#include "WiFi.h"
#include "FS.h"
#include "SPIFFS.h"
#include "ESPAsyncWebServer.h"
#include "picture.h"

AsyncWebServer webServer(80);
// TODO: Refactor into system settings or config files
#define SCREEN_SIZE_X 800
#define SCREEN_SIZE_Y 480
#define PAGE_STEPS 10
#define SCREEN_RESOLUTION SCREEN_SIZE_X*SCREEN_SIZE_Y/8   // Display size in pixels.
#define PAGE_INCREMENT_SIZE SCREEN_SIZE_Y/PAGE_STEPS      // Number of pixels per page, 48 = 10 pages on 480px display
#define PAGE_BYTE_SIZE SCREEN_RESOLUTION/PAGE_STEPS

//#include "server.h"

//#include <GxEPD.h>
//#include <GxGDEW075T7/GxGDEW075T7.h>
//#include <GxIO/GxIO_SPI/GxIO_SPI.h>
//#include <GxIO/GxIO.h>
//GxIO_Class io(SPI, /*CS=5*/ 15, /*DC=*/ 27, /*RST=*/ 27); // Pins of Waveshark ESP32 display driver
//GxEPD_Class display(io, /*RST=*/ 27, /*BUSY=*/ 25);

// For GxEPD2 - currently not working, overflow?
// #define ENABLE_GxEPD2_GFX 0
 #include <GxEPD2_BW.h>
 //#define MAX_DISPLAY_BUFFER_SIZE 15000ul // ~15k is a good compromise
 //#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8) ? EPD::HEIGHT : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8))
 //GxEPD2_BW<GxEPD2_750_T7, MAX_HEIGHT(GxEPD2_750_T7)> display(GxEPD2_750_T7(/*CS=4*/ 15, /*DC=*/ 27, /*RST=*/ 26, /*BUSY=*/ 25)); // GDEW075T7 800x480
 //GxEPD2_BW < GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT / 2 > display(GxEPD2_750_T7(/*CS=D8*/ 15, /*DC=D3*/ 27, /*RST=D4*/ 26, /*BUSY=D2*/ 25)); // GDEW075T7 800x480
GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT> display(GxEPD2_750_T7(/*CS=*/ 15, /*DC=*/ 27, /*RST=*/ 26, /*BUSY=*/ 25));

// == CONFIG ==
/*
   Remember not to commit your credentials on github :P
   Using ignored file instead
*/
#include "local_dev_config.h" // optional local dev settings, see readme
#ifndef wifiSettings
const char* ssid = "My Wifi Network";
const char* password = "My totally secure password";
#endif

void handleUpload(AsyncWebServerRequest * request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index) {
    Serial.printf("UploadStart: %s\n", filename.c_str());
  }
  for (size_t i = 0; i < len; i++) {
    Serial.print(data[i]);
  }
  if (final) {
    Serial.printf("UploadEnd: %s, %u B\n", filename.c_str(), index + len);
  }
}



String getMemoryUsage() {
  String usage = String(ESP.getFreeHeap());
  return usage;
}

String getFullMemoryUsage() {
  String usage = "Total heap: ";
  usage = String(usage + ESP.getHeapSize());
  usage = String(usage + "\nFree heap: ");
  usage = String(usage + String(ESP.getFreeHeap()));
  usage = String(usage + "\nTotal PSRAM: ");
  usage = String(usage + String(ESP.getPsramSize()));
  usage = String(usage + "\nFree PSRAM: ");
  usage = String(usage + String(ESP.getFreePsram()));
  return usage;
}

/**
 * Handles multipart file upload and save the file on spiffs.
 * Modified from source: https://github.com/Edzelf/Esp-radio
 * MIT License
 * Copyright (c) 2019 Ed Smallenburg
 **/
void handleFileUpload ( AsyncWebServerRequest *request, String filename,
                        size_t index, uint8_t *data, size_t len, bool final )
{
  Serial.println("upload handle starting.");
  String          path ;                              // Filename including "/"
  static File     f ;                                 // File handle output file
  char*           reply ;                             // Reply for webserver
  static uint32_t t ;                                 // Timer for progress messages
  uint32_t        t1 ;                                // For compare
  static uint32_t totallength ;                       // Total file length
  static size_t   lastindex ;                         // To test same index

  if ( index == 0 )
  {
    path = String ( "/" ) + filename ;                // Form SPIFFS filename
    SPIFFS.remove ( path ) ;                          // Remove old file
    f = SPIFFS.open ( path, "w" ) ;                   // Create new file
    t = millis() ;                                    // Start time
    totallength = 0 ;                                 // Total file lengt still zero
    lastindex = 0 ;                                   // Prepare test
  }
  t1 = millis() ;                                     // Current timestamp
  // Yes, print progress
  printf ( "File upload %s, t = %d msec, len %d, index %d",
               filename.c_str(), t1 - t, len, index ) ;
  if ( len )                                          // Something to write?
  {
    if ( ( index != lastindex ) || ( index == 0 ) )   // New chunk?
    {
      f.write ( data, len ) ;                         // Yes, transfer to SPIFFS
      totallength += len ;                            // Update stored length
      lastindex = index ;                             // Remenber this part
    }
  }
  if ( final )                                        // Was this last chunk?
  {
    f.close() ;                                       // Yes, close the file

    request->send ( 200, "" ) ;
  }
}

void startServer() {
  /*
     Hardcoded requests for all needed angular files.
     TODO: Replace with handler for all root requests
  */
  webServer.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/www/index.html", "text/html");
  });
  webServer.on("/index.html", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/www/index.html", "text/html");
  });
  webServer.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/www/favicon.ico", "text/html");
  });

  webServer.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/www/styles.css.gz", "text/css");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });
  webServer.on("/runtime.js", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/www/runtime.js.gz", "application/javascript");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });
  webServer.on("/polyfills.js", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/www/polyfills.js.gz", "application/javascript");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });
  webServer.on("/main.js", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/www/main.js.gz", "application/javascript");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  /**
     Get free memory
  */
  webServer.on("/api/system/heap", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(200, "text/plain", getMemoryUsage());
  });

  webServer.on("/api/image/upload-single", HTTP_POST, [](AsyncWebServerRequest *request){
    //request->send(200);
  }, handleFileUpload);

  // webServer.onFileUpload(handleFileUpload);

  webServer.begin();
}

void startWifi() {
  WiFi.begin(ssid, password);
  delay(1000);
  WiFi.mode( WIFI_OFF );
  delay(500);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  delay(100);
  Serial.println(WiFi.localIP());
}

void wipeDisplay() {
  Serial.println("Initializing e-ink display...");
  display.setRotation(0);
  //display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setFullWindow();
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
  }
  while (display.nextPage());
}

// == Functions ==
void initDisplay_old() {
  display.init(115200); // enable diagnostic output on Serial
  display.fillScreen(GxEPD_BLACK);
  display.fillScreen(GxEPD_WHITE);
}

void initDisplaySpi() {
  display.init(0); // 0 avoids this lib to spawn it's own Serial object
  // uses standard SPI pins, e.g. SCK(18), MISO(19), MOSI(23), SS(5)
  // *** special handling for Waveshare ESP32 Driver board *** //
  // ********************************************************* //
  SPI.end(); // release standard SPI pins, e.g. SCK(18), MISO(19), MOSI(23), SS(5)
  //SPI: void begin(int8_t sck=-1, int8_t miso=-1, int8_t mosi=-1, int8_t ss=-1);
  SPI.begin(13, 12, 14, 15); // map and init SPI pins SCK(13), MISO(12), MOSI(14), SS(15)
  // *** end of special handling for Waveshare ESP32 Driver board *** //
  // **************************************************************** //
}

void listDir(fs::FS &fs, const char * dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\r\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("- failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println(" - not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.name(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("\tSIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void listFiles()
{
  listDir(SPIFFS, "/", 0);
}

void drawTestPicture()
{
  display.setFullWindow();
  display.clearScreen(); // use default for white
  display.drawImage(picture, 0, 0, 800, 480, true, false, true);
}

void setup() {
  initDisplaySpi();
  //Serial.begin(115200);
  wipeDisplay();
  if (!SPIFFS.begin()) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  startWifi();
  startServer();
  listFiles();
  drawTestPicture();
}

void loop() {}


