/**
 * @note
 ** Timing Point Code (TPC)
 *  Essentially refers to a specific geographical stop, whereas a timingPointName, e.g. "Arent Krijtsstraat", has two stops, 
 *  one for each direction. The direction is identified by the field "LineDirection" and is e.g. "1" or "2". 
 * 
 ** Customisation options for configuring for a different Time_Planning_Code
 *  To find the TPC of a specific stop: 
 *      1.  Search stop name (aka timing point name) in [stops.txt] file in the .zip file [gtfs-nl.zip] available at https://gtfs.ovapi.nl/.
 *      2.  If more than one entry exists, you can validate if correct (includes extra info):
 *                  - Search the stop_code from [stops.txt] against the TPC at http://v0.ovapi.nl/tpc. The stop_code appears to be the TPC,
 *                    but is often shortened. E.g. 00509 instead of 30000509.
 *                  - Check upcoming services match desired stop. 
 *                      - Data by Timing Point Name and Journey is also updated to https://drgl.nl (HTML), e.g.:
 *                          - By Timing Point Name: https://drgl.nl/stop/NL:S:30000504  and https://drgl.nl/stop/NL:S:30000504/departurespanel
 *                               - NL:S: might be e.g. a region code. The data covers NL and Belgium.
 *                               - 30000504 appears to be a code for the TimingPointName "Arent Krijtsstraat", but doesn't appear to be a TPC. 
 *                                 No other reference was found anywhere, so maybe it's created just for that website by combining both stops with 9-5=4:
 *                                      1. TPC 30000505 is LineDirection = 1, which goes away from Amsterdam
 *                                      2. TPC 30000509 is LineDirection = 2, which goes towards Amsterdam
 *                          - Select a journey, and it opens e.g. https://drgl.nl/journey/GVB:19:357/20250512/ 
 *                                -   Uses form: https://drgl.nl/journey/operatorCode:linePublicNumber:journeyNumber:LocalServiceLevelCode
 * @see
 ** OVapi (Server directory)   
 *  - Check https://gtfs.ovapi.nl/README    before using
 *  - https://gtfs.ovapi.nl/                See gtfs-nl.zip for GTFS data in .txt files   
 *  - http://v0.ovapi.nl/tpc/               JSON
 *  - http://v0.ovapi.nl/line/              JSON
 *  - https://v0.ovapi.nl/journey/          JSON
 *  - https://v0.ovapi.nl/stopareacode/     JSON
 * 
 ** DRGL (HTML site)
 *  - https://drgl.nl/stop/NL:S:30000504/departurespanel
 *  - https://drgl.nl/journey/GVB:19:357/20250512/
*/

// TESTING ONLY - SPACING OF LINES ON EPD FOR LAYOUT
//#define ySpacing 500
//#define xSpacing 500

// ===========================
// LIBRARIES
// ===========================
                  
#include <Arduino.h>

#include <WiFi.h>           // for accessing data source on servers
#include <HTTPClient.h>     // for accessing data source on servers
#include <ArduinoJson.h>    // for reading & extracting data
#include <vector>           // for sorting by departuretime 
#include <algorithm>        // for sorting by departuretime 
#include <time.h>           // for getting current time
#include "driver/rtc_io.h"


// DISPLAY
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <GxEPD2_4C.h>
#include <GxEPD2_7C.h>
//#include <select_fonts.h>               // a list of includes with all fonts

// Fonts used
#include <fonts/Ubuntu_R_6pt8b.h>
#include <fonts/Ubuntu_R_7pt8b.h>
#include <fonts/Org_01.h>
#include <fonts/FreeSansBold8pt7b.h>



// ===========================
// CUSTOM OPTIONS
// ===========================

// SSIDs and Passwords
const char* ssids[] = {"SSID_1_name", "SSID_2_name"};
const char* passwords[] = {"ssid1password", "ssid2password"};

// Custom TPC (stop + direction) for which to obtain upcoming services
// data source: format is http://v0.ovapi.nl/tpc/[time-planning-code] where TPC is the value representing the stop+direction (specific stop location)
const char* endpoint = "http://v0.ovapi.nl/tpc/30000509";  // ENTER YOUR SPECIFIC TIME PLANNING CODE

// Display selection
GxEPD2_BW<GxEPD2_213_BN, GxEPD2_213_BN::HEIGHT> display(GxEPD2_213_BN(/*CS=D8*/ 5, /*DC=D3*/ 17, /*RST=D4*/ 16, /*BUSY=D2*/ 4)); // DEPG0213BN 122x250, SSD1680, TTGO T5 V2.4.1, V2.3.1

// Deep-sleep button pin declarations 
#define BUTTON_PIN GPIO_NUM_33      // Must be an RTC GPIO. GPIO33 is RTC-capable.

#define BUTTON_PIN_BITMASK(GPIO) (1ULL << GPIO)  // 2 ^ GPIO_NUMBER in hex
#define USE_EXT0_WAKEUP          1               // 1 = EXT0 wakeup, 0 = EXT1 wakeup
#define WAKEUP_GPIO              GPIO_NUM_33     // Only RTC IO are allowed - ESP32 Pin example

// Customise departure board title on line 648: display.print("MY UPCOMING DEPARTURES");


// ===========================
// Global Structs 
// ===========================

  struct TramPass {
    int journeyNumber;
    String linePublicNumber;
    String departureTime;   // uses full ISO 8601 timestamp e.g., "2025-05-12T23:08:45"
    String tripStatus;
    time_t departureEpoch;  // for sorting & countdown
    String transportType;
    String destinationName50;
    String destinationCode;
    String operatorCode;
    String timingPointName;
};


// ===========================
// Global Variables
// ===========================

String nextDepartureTimeStr;  // formatted as HH:MM
String soonestDepartureStr = "";
time_t nextDepartureEpoch = 0;
String linePublicNumber = ""; 
int journeyNumber = 0;
bool hasDeparture = false;


// ===========================
// OVapi.nl requirements                         
// ===========================
// Store globally or in preferences (EEPROM or SPIFFS??) if/as needed

#include <Preferences.h>    // for preventing download from endpoint if data is unchanged
Preferences prefs;
String savedETag = "";
String savedLastModified = "";

// ===========================
// Custom icons for display (global variables)
// ===========================

#define ICON_TRAM_WIDTH 9
#define ICON_TRAM_HEIGHT 12
const unsigned char PROGMEM icon_tram[] = {
  0b01111111, 0b00000000,
  0b00001000, 0b00000000,
  0b00111110, 0b00000000,
  0b11111111, 0b10000000,
  0b10000000, 0b10000000,
  0b10000000, 0b10000000,
  0b10000000, 0b10000000,
  0b11111111, 0b10000000,
  0b11110111, 0b10000000,
  0b01111111, 0b00000000,
  0b00100010, 0b00000000,
  0b01000001, 0b00000000
};

#define ICON_BUS_WIDTH 9
#define ICON_BUS_HEIGHT 11
const unsigned char PROGMEM icon_bus[] = {
  0b01111111, 0b00000000,
  0b11111111, 0b10000000,
  0b10000000, 0b10000000,
  0b10000000, 0b10000000,
  0b10000000, 0b10000000,
  0b11111111, 0b10000000,
  0b11111111, 0b10000000,
  0b11011101, 0b10000000,
  0b01111111, 0b00000000,
  0b01000001, 0b00000000,
  0b00000000, 0b00000000
  };


// ===========================
// Function Declarations
// ===========================

void connectToWifi();
void displayTimeTable(const std::vector<TramPass>& trams);
void print_wakeup_reason();
bool tryWiFiConnection(int ssidIndex);
bool waitForTimeSync(int timeoutSeconds);
bool performHttpGet();
void parseJSON(const String& json);
void setupTime();
void loadSavedHeaders();
void saveHeaders(const String& etag, const String& lastModified);

// ===========================
// DIAGNOSTICS                    (used for printing diagnostics in partial refresh 
// ===========================

enum DiagnosticState {
  WIFI_FAIL,
  TIME_FAIL,
  HTTP_FAIL,
  NO_UPDATE_ETAG,
  NO_UPDATE_LASTMOD,
  STATUS_OK
};

DiagnosticState diagState = STATUS_OK;

void showDiagnostic(DiagnosticState state); 


// ===========================
// void setup() Function
// ===========================


void setup() {

  Serial.begin(115200);
  display.init(115200, true, 2, false); // USE THIS for Waveshare boards with "clever" reset circuit, 2ms reset pulse

  pinMode(BUTTON_PIN, INPUT_PULLUP);  // So it's HIGH when button not pressed
  esp_sleep_enable_ext0_wakeup(BUTTON_PIN, 0);  // Wake when GPIO is LOW

    delay(200);

  //Increment boot number and print it every reboot
  
  // ++bootCount;
  // Serial.println("Boot number: " + String(bootCount));
  
  prefs.begin("boot-count", false);
  int bootCount = prefs.getInt("count", 0);
  bootCount++;
  prefs.putInt("count", bootCount);
  prefs.end();

  Serial.println("Boot count: " + String(bootCount));

  print_wakeup_reason();   // For testing - print the reason for the ESP32's wakeup
  
  int bestIndex = -1;
  int bestRSSI = -1000;

  Serial.println("Scanning networks...");
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) {
    Serial.printf("Found SSID: %s, RSSI: %d\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
    for (int j = 0; j < 2; j++) {
      if (WiFi.SSID(i) == ssids[j] && WiFi.RSSI(i) > bestRSSI) {
        bestIndex = j;
        bestRSSI = WiFi.RSSI(i);
      }
    }
  }

  if (bestIndex == -1) {
    Serial.println("No preferred networks found. Going to sleep.");
    showDiagnostic(WIFI_FAIL);  // indicate failure to find any preferred networks
    esp_deep_sleep_start();
  }

  if (!tryWiFiConnection(bestIndex) || !waitForTimeSync(10) || !performHttpGet()) {
    Serial.println("Trying fallback network...");
    int fallbackIndex = 1 - bestIndex;  // assumes exactly 2 networks
    if (!tryWiFiConnection(fallbackIndex) || !waitForTimeSync(20) || !performHttpGet()) {
      Serial.println("Both networks failed. Going to sleep.");
      showDiagnostic(WIFI_FAIL);  // indicate failure connecting or syncing or HTTP
      esp_deep_sleep_start();
    }
  }
 

    delay(2000);

  Serial.println("Going to sleep now"); // for testing
  Serial.flush();  // Ensure all serial data isu sent before going to sleep.
  esp_deep_sleep_start();  // Put the ESP32 into deep sleep mode.
}
    
// ===========================
// MACROS
// ===========================
  
bool tryWiFiConnection(int ssidIndex) {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("NXT No19 Updater");
  WiFi.begin(ssids[ssidIndex], passwords[ssidIndex]);
  Serial.printf("Connecting to %s...\n", ssids[ssidIndex]);

  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  // set diagState to No wifi
  if (WiFi.status() != WL_CONNECTED) {
    diagState = WIFI_FAIL;
    showDiagnostic(diagState);
    return false;
  }
  return true;
}

bool waitForTimeSync(int timeoutSeconds) {
  setupTime();
  Serial.print("Waiting for time sync");
  unsigned long start = millis();

  while (time(nullptr) < 100000 && (millis() - start) < timeoutSeconds * 1000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (time(nullptr) < 100000) {
    diagState = TIME_FAIL;
    showDiagnostic(diagState);
    return false;  // must return a bool value here!
  }

  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  if (timeinfo) {
    char buffer[26];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    Serial.print("Current time: ");
    Serial.println(buffer);
  } else {
    Serial.println("Failed to obtain current time");
  }

  return true;
}


    void loadSavedHeaders() {    //   On startup/first use, loads the last-known ETag and Last-Modified headers from persistent storage.
        prefs.begin("http-cache", true);  // Read-only
        if (prefs.isKey("etag")) {
          savedETag = prefs.getString("etag", "");    // checks to see if etag exists - if not, doesn't try logging what isn't there/printing serial warnings
        }
        if (prefs.isKey("lastmod")) {
          savedLastModified = prefs.getString("lastmod", "");
        }
        prefs.end();
      }

    void saveHeaders(const String& etag, const String& lastModified) { // When the server responds with new headers, saveHeaders() writes them back to flash.
      prefs.begin("http-cache", false);  // Write mode
      if (etag != "") {
        prefs.putString("etag", etag);
      }
      if (lastModified != "") {
        prefs.putString("lastmod", lastModified);
      }
      prefs.end();
    }

    bool performHttpGet() {
      HTTPClient http;
      http.setUserAgent("[ENTER APP/FIRMWAR NAME HERE] ([ENTER YOUR EMAIL HERE ])");  // OVapi.nl requirement - replace with your app name and email
    
      loadSavedHeaders();
    
      if (savedETag.length() > 0) {
        http.addHeader("If-None-Match", savedETag);
      }
      if (savedLastModified.length() > 0) {
        http.addHeader("If-Modified-Since", savedLastModified);
      }
    
      http.begin(endpoint);
      int httpCode = http.GET();
    
      String newLastModified = http.header("Last-Modified");
      saveHeaders(http.header("ETag"), newLastModified);
    
      if (httpCode == HTTP_CODE_NOT_MODIFIED) {
        diagState = NO_UPDATE_ETAG;
        http.end();
        showDiagnostic(diagState);
        return false;
      }
    
      if (httpCode <= 0) {
        diagState = HTTP_FAIL;
        showDiagnostic(diagState);
        http.end();  // Don't forget to end the session
        return false;
      }
    
      // Fetch response
      String payload = http.getString();
    
      Serial.println("Received JSON:");
      Serial.println(payload);  // Optional for debug
    
      diagState = STATUS_OK;
      showDiagnostic(diagState);
    
      http.end();
    
      // Parse and render
      parseJSON(payload);
    
      return true;
    }
    

void setupTime() {
  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "europe.pool.ntp.org");
}


void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:     Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1:     Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER:    Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD: Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP:      Serial.println("Wakeup caused by ULP program"); break;
    default:                        Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); break;
  }
}
    
    void parseJSON(const String& json) {
        const size_t capacity = 32 * 1024;
        ArduinoJson::DynamicJsonDocument doc(capacity);
    
        DeserializationError error = deserializeJson(doc, json);
        if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.f_str());
        return;
        }
    
        JsonObject passes = doc["30000509"]["Passes"];
        std::vector<TramPass> trams;
    
        time_t soonestEpoch = 0;
    
        for (JsonPair kv : passes) {
        JsonObject tram = kv.value().as<JsonObject>();
        int journeyNumber = tram["JourneyNumber"];
        const char* linePublicNumber = tram["LinePublicNumber"];
        const char* departureStr = tram["ExpectedArrivalTime"];
        const char* tripStatus = tram["TripStopStatus"];
        
        const char* transportType = tram["TransportType"];
        const char* destinationName50 = tram["DestinationName50"];
        const char* destinationCode = tram["DestinationCode"];
        const char* operatorCode = tram["OperatorCode"];
        const char* timingPointName = tram["TimingPointName"];


        // Parse time to time_t
        struct tm tm{};
        
        strptime(departureStr, "%Y-%m-%dT%H:%M:%S", &tm);

        tm.tm_isdst = -1;  // let the system figure Daylight Savings Time out or not? 

        time_t epoch = mktime(&tm);

        // Add to vector
        trams.push_back({
          journeyNumber,
          String(linePublicNumber),
          String(departureStr),
          String(tripStatus),
          epoch,
          String(transportType),
          String(destinationName50),
          String(destinationCode),
          String(operatorCode),
          String(timingPointName)
        });


        // Track soonest
        if (soonestEpoch == 0 || epoch < soonestEpoch) {
            soonestEpoch = epoch;
        }
        }
    
        // Sort trams by departureEpoch
        std::sort(trams.begin(), trams.end(), [](const TramPass& a, const TramPass& b) {
        return a.departureEpoch < b.departureEpoch;
        });
    
        // Print sorted list
        for (const auto& tram : trams) {
        int tIndex = tram.departureTime.indexOf('T');
        String hhmm = tram.departureTime.substring(tIndex + 1, tIndex + 6);
    
        Serial.print("Service: ");
        Serial.print(tram.transportType);

        Serial.print(" | Operator: ");
        Serial.print(tram.operatorCode);

        Serial.print(" | Journey: ");
        Serial.print(tram.journeyNumber);

        Serial.print(" | Line: ");
        Serial.print(tram.linePublicNumber);

        Serial.print(" | From: ");
        Serial.print(tram.timingPointName);

        Serial.print(" | To: ");
        Serial.print(tram.destinationName50);

        Serial.print(" (Code: ");
        Serial.print(tram.destinationCode);

        Serial.print(") | Departure: ");
        Serial.print(hhmm);

        Serial.print(") | Time until Depart: ");


        // Print time until departure
        time_t now = time(nullptr);                                  // time(nullptr) gives the current local epoch time (set up by setupTime()).
        int secondsUntilDeparture = tram.departureEpoch - now;       // tram.departureEpoch contains the parsed scheduled time in epoch seconds.

        if (secondsUntilDeparture < 0) {
          Serial.print("Departed");
        } else {
          int minutes = secondsUntilDeparture / 60;
          int seconds = secondsUntilDeparture % 60;

          char timeLeft[6]; // MM:SS
          snprintf(timeLeft, sizeof(timeLeft), "%02d:%02d", minutes, seconds);  // Formatting with snprintf() provides a padded MM:SS string.
          Serial.print(timeLeft);
        }

        Serial.print(" | Status: ");
        Serial.println(tram.tripStatus);
        }

    
        // Save earliest for countdown in loop()
        if (!trams.empty()) {
        nextDepartureEpoch = trams[0].departureEpoch;
        journeyNumber = trams[0].journeyNumber;
        linePublicNumber = trams[0].linePublicNumber;
        hasDeparture = true;
        
        // save departure time of soonest tram
        soonestDepartureStr = trams[0].departureTime;
        int tIndex = soonestDepartureStr.indexOf('T');
        nextDepartureTimeStr = soonestDepartureStr.substring(tIndex + 1, tIndex + 6);    // HH:MM
    
        } else {
        hasDeparture = false;
        }

        // Update the ePaper display
        displayTimeTable(trams);
    
    }


    void displayTimeTable(const std::vector<TramPass>& trams)
    {
    display.setRotation(1);  // Landscape
    display.setFullWindow();
    display.firstPage();

    const int marginLeft = 0;
    const int marginTop = 10;
    const int lineHeight = 15;

    do {
        display.fillScreen(GxEPD_WHITE);

        // Title
        display.fillRect(0,0,250,12,GxEPD_BLACK);
        display.setFont(&Ubuntu_R_7pt8b); //FreeSans_5pt8b);
        display.setCursor(2, marginTop);
        display.setTextColor(GxEPD_WHITE);

        display.print("MY UPCOMING DEPARTURES");

// Column headers
        display.setFont(&Ubuntu_R_6pt8b); // FreeSans_6pt8b);
        display.setTextColor(GxEPD_BLACK);

        display.setCursor(1, marginTop+13);
        display.print("LINE");

        display.setCursor(48, marginTop+13);
        display.print("DEPART");

        display.setCursor(103, marginTop+13);
        display.print("(MIN:SEC)");

        display.setCursor(172, marginTop+13);
        display.print("STATUS");

        /* REMOVED TO CREATE SPACE. This was the header, below which the "To" station code was displayed9
        display.setCursor(210, marginTop+15);
        display.print("TO");    
        */

        // Draw lines to visually seperate column headers from content
        display.drawLine(marginLeft,28,250-marginLeft,28,GxEPD_BLACK);

        display.setFont(&FreeSansBold8pt7b);

        // Tram lines
        for (size_t i = 0; i < trams.size() && i < 5; ++i) {
          const TramPass& tram = trams[i];
          int tIndex = tram.departureTime.indexOf('T');
          String hhmm = tram.departureTime.substring(tIndex + 1, tIndex + 6);
      
          int y = marginTop + 18 + (i + 1) * lineHeight;
      
          // DRAW ICON - based on transport type ****Adjust the pixel offsets + 2 / + # values for alignment as needed
        if (tram.transportType == "TRAM") {
            display.drawBitmap(marginLeft, y - ICON_TRAM_HEIGHT + 2, icon_tram, ICON_TRAM_WIDTH, ICON_TRAM_HEIGHT, GxEPD_BLACK);
        } else if (tram.transportType == "BUS") {
            display.drawBitmap(marginLeft, y - ICON_BUS_HEIGHT + 2, icon_bus, ICON_BUS_WIDTH, ICON_BUS_HEIGHT, GxEPD_BLACK);
        }
      
          // DISPLAY LINE NUMBER - set text after icon
        int textX = marginLeft + ICON_TRAM_WIDTH + 4; // add padding after icon
        display.setCursor(textX, y);
        display.print(tram.linePublicNumber);

          // DISPLAY DEPARTURE TIME
//          display.setCursor(50, y);
//          display.print(hhmm);



          // DISPLAY DEPARTURE TIME & TIME UNTIL DEPARTURE e.g.   "17:12 (04:21)"
              // Get current time
        time_t now = time(nullptr);
        int secondsUntil = tram.departureEpoch - now;

        if (secondsUntil < 0) {

            // If tram has departed                                  "Departed"
            display.setCursor(50, y);
            display.print(hhmm);

            display.setCursor(105, y);
            display.print("(    -    )");

        } else {
                // Format countdown
            int minutes = secondsUntil / 60;
            int seconds = secondsUntil % 60;
            char countdown[10];
            snprintf(countdown, sizeof(countdown), "%02d:%02d", minutes, seconds);

            // Display HH:MM and countdown
            display.setCursor(50, y);
            display.print(hhmm);                   //     e.g.    "17:12"
            
            display.setCursor(105, y);
            display.print("(");
            display.print(countdown);              //     e.g.    "(08:10)"
            display.print(")");
        }

        display.setCursor(172, y);
        display.print(tram.tripStatus);           //    e.g.    "Planned"

        /* REMOVED TO CREATE SPACE. This would display the "To" station code.
        display.setCursor(210, y);
        display.print(tram.destinationCode);    // E.g. NCS = Centraal Station, SLD = Station Sloterdijk
        */
        
        }

        // Draw black rectangle footer bar
        display.fillRect(0, 109, 250, 20, GxEPD_BLACK);


        // Set font and text color for time
        //display.setFont(&FreeSans_5pt8b);
        display.setTextColor(GxEPD_WHITE);

        // Get current time
        time_t now = time(nullptr);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);

        if (timeinfo.tm_year > 100) {  // Basic sanity check: year > 2000
            char timeStr[9];  // HH:MM:SS
            strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);

            // Print time string to display
            display.setFont(&Ubuntu_R_7pt8b); // set font for title
            display.setCursor(110, 119);  // adjust position as needed
            display.print("UPDATED:");
          // display.setFont(&FreeSans_6pt8b); // set font for time
            display.setCursor(190, 119);  // adjust position as needed
            display.print(timeStr);
        } else {
            display.setFont(&Ubuntu_R_7pt8b); // set font for title
            display.setCursor(180, 119);
            display.print("No Time");
        } 

/*      for (int y = 0; y <= 122; y += ySpacing) {
//          display.writeLine(0, y, 250, y, GxEPD_BLACK); // Horizontal lines
//          }
//        for (int x = 0; x <= 250; x += xSpacing) {
//          display.writeLine(x, 0, x, 122, GxEPD_BLACK); // Vertical lines
        }*/

    } while (display.nextPage());

    showDiagnostic(diagState);
  }

  void showDiagnostic(DiagnosticState state) {
    display.setRotation(1);
    display.setPartialWindow(0, 112, 110, 24); // Top line
    
    do {
      display.fillScreen(GxEPD_WHITE);
      display.fillRect(0, 109, 110, 20, GxEPD_BLACK);
      display.setCursor(3, 117);
      display.setTextColor(GxEPD_WHITE);
      display.setFont(&Org_01); // set font for title // alternative Ubuntu_R_5pt8b.h
      

      switch (state) {
        case WIFI_FAIL:
          display.print("No wifi");
          break;
        case TIME_FAIL:
          display.print("No network time");
          break;
        case HTTP_FAIL:
          display.print("OVapi server GET fail");
          break;
        case NO_UPDATE_ETAG:
          display.print("Nothing to update");
          break;
        case NO_UPDATE_LASTMOD:
          display.print("Already updated in last 60s");
          break;
        case STATUS_OK:
          display.print("Updated OK");
          break;
      }
    } while (display.nextPage());
  }

void loop() {

}

