// Define a higher nesting limit to handle deep JSON structures
#define ARDUINOJSON_DEFAULT_NESTING_LIMIT 16

// ------------------------------------------------------------------------------------
// WIRING INSTRUCTIONS: Arduino Nano ESP32 to WeAct 4.2" Epaper
// ------------------------------------------------------------------------------------
// ePaper VCC  -> Nano ESP32 3V3 OUT
// ePaper GND  -> Nano ESP32 GND
//
// SPI Communication:
// ePaper DIN  -> Nano ESP32 D11 (GPIO11) (SPI MOSI)
// ePaper CLK  -> Nano ESP32 D13 (GPIO12) (SPI SCK)
//
// Control Pins (Ensure these GPIO numbers match your wiring):
// ePaper CS   -> Connect to the pin corresponding to GPIO5 on your Nano ESP32 (e.g., D2)
// ePaper DC   -> Connect to the pin corresponding to GPIO0 on your Nano ESP32 (Note: GPIO0 can be tricky, often related to boot mode)
// ePaper RST  -> Connect to the pin corresponding to GPIO2 on your Nano ESP32
// ePaper BUSY -> Connect to the pin corresponding to GPIO15 on your Nano ESP32
//
// Important: The GxEPD2 constructor below uses raw GPIO numbers.
// Please verify that you have wired CS to GPIO5, DC to GPIO0, RST to GPIO2, and BUSY to GPIO15.
// The Nano ESP32 pin D2 is GPIO5. GPIO0, GPIO2, GPIO15 are available but not always on standard D-pin headers.
// Double-check your specific Nano ESP32 variant's pinout for GPIO0, GPIO2, and GPIO15 if you are unsure.
// ------------------------------------------------------------------------------------

// Include necessary libraries
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

// ePaper Display Libraries
#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>

// Initialize the display for the 4.2" ePaper module using GPIO numbers
// These are the pin numbers from your working code.
GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT> display(GxEPD2_420_GDEY042T81(
  /*CS=*/   5,    // Chip Select pin (GPIO5)
  /*DC=*/   0,    // Data/Command pin (GPIO0)
  /*RST=*/  2,    // Reset pin (GPIO2)
  /*BUSY=*/ 15    // Busy pin (GPIO15)
  // Hardware SPI pins (SCK, MOSI/SDA) are typically used by default by the library.
  // For Arduino Nano ESP32: SCK is GPIO12 (D13), MOSI is GPIO11 (D11).
));
// ------------------------------------------------------------------------------------

// WiFi credentials (replace with your own if different)
const char* ssid = "Yunes";
const char* password = "123456789";

// API details
const char* apiKey = "9b00b65e-e873-45af-8ff8-47366a137f53";
const char* stopIds = "1550|1583";

// Filter for JSON parsing to optimize memory usage
StaticJsonDocument<256> filter;

// Define stop names (as they appear in your original code)
String stopName1550 = "Gammel Kongevej (H.C. Ørsteds Vej)";
String stopName1583 = "Gammel Kongevej (Alhambravej)";

void setup() {
  // Initialize serial communication for debugging
  Serial.begin(115200);
  delay(100); // Wait for serial to initialize

  Serial.println("Starting E-Paper Bus Schedule Display (Rotated View)...");

  // Initialize the display
  // Parameters: serial_diag_bitrate, initial_cp, wake_up_delay, spi_try_optimized
  display.init(115200, true, 10, false);
  display.setRotation(0); // Set rotation to 0 for native landscape (400px width, 300px height)
                          // Previous was 3.

  // Initial display message
  display.setFont(&FreeMonoBold12pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(5, 30);
    display.print("Initializing...");
  } while (display.nextPage());

  // Connect to WiFi
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  int wifi_retries = 0;
  while (WiFi.status() != WL_CONNECTED && wifi_retries < 30) { // Max 30 retries (30 seconds)
    delay(1000);
    Serial.print(".");
    wifi_retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" Connected!");
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setCursor(5, 30);
        display.print("WiFi Connected!");
    } while (display.nextPage());
  } else {
    Serial.println(" WiFi Connection Failed!");
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setCursor(5, 30);
        display.print("WiFi Failed!");
        display.setCursor(5, 50);
        display.print("Check credentials.");
    } while (display.nextPage());
    // Halt or retry indefinitely as per your requirement
    while(true) delay(1000);
  }

  // Set time via NTP (UTC+2 for CEST)
  Serial.println("Configuring time via NTP...");
  configTime(7200, 0, "pool.ntp.org"); // 7200 seconds = UTC+2

  // Wait for time to synchronize
  time_t now_check = time(nullptr);
  while (now_check < 100000) { // Basic check for valid timestamp (epoch time > early 1970s)
    delay(1000);
    Serial.println("Waiting for time sync...");
    now_check = time(nullptr);
  }
  Serial.println("Time synchronized.");

  // Define the filter for JSON parsing
  JsonObject arrivalFilter = filter["Arrival"].createNestedObject();
  arrivalFilter["stopExtId"] = true;  // Stop ID
  arrivalFilter["name"] = true;       // Line name
  arrivalFilter["time"] = true;       // Scheduled time
  arrivalFilter["date"] = true;       // Scheduled date
  arrivalFilter["rtTime"] = true;     // Real-time time (if available)
  arrivalFilter["rtDate"] = true;     // Real-time date (if available)

  Serial.println("Setup complete.");
}

void loop() {
  Serial.println("Fetching new bus data...");

  // Get current time
  time_t now = time(nullptr);
  struct tm *localTime = localtime(&now);

  // Extract current date and time
  char currentDate[11]; // YYYY-MM-DD
  strftime(currentDate, sizeof(currentDate), "%Y-%m-%d", localTime);
  char currentTime[6]; // HH:MM
  strftime(currentTime, sizeof(currentTime), "%H:%M", localTime);
  int currentHour = localTime->tm_hour;
  int currentMinute = localTime->tm_min;
  int currentMinutes = currentHour * 60 + currentMinute; // Current time in minutes since midnight

  // Construct API URL
  String url = "https://www.rejseplanen.dk/api/multiArrivalBoard?idList=" + String(stopIds) +
               "&date=" + currentDate + "&time=" + currentTime + "&accessId=" + apiKey + "&format=json";

  Serial.println("API URL: " + url);

  // Variables to store display strings
  String nextArrival1550_display = "No buses today";
  String nextArrival1583_display = "No buses today";
  String arrivalTime1550_val = "";
  String arrivalTime1583_val = "";

  // Make HTTP request
  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    // Serial.println("Payload: " + payload); // For debugging, can be very long

    // Parse JSON with filter
    StaticJsonDocument<4096> doc; // Increased size for potentially larger payloads
    DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));

    if (error) {
      Serial.print("JSON parsing error: ");
      Serial.println(error.c_str());
      nextArrival1550_display = "Data error";
      nextArrival1583_display = "Data error";
    } else {
      JsonArray arrivals = doc["Arrival"];
      if (arrivals.isNull() || arrivals.size() == 0) {
        Serial.println("No arrival data in JSON response.");
        // 'No buses today' will remain if no specific arrivals are found
      }

      // Initialize variables for next arrivals (use a large value to detect no arrivals)
      int nextTime1550_calc = 1440 * 2;  // Max minutes for two days
      int nextTime1583_calc = 1440 * 2;

      for (JsonObject arrival : arrivals) {
        String stopExtId = arrival["stopExtId"].as<String>();
        String line = arrival["name"].as<String>();

        // Only process arrivals for "Bus 1A"
        if (line != "Bus 1A") continue;

        String arrivalDateStr = arrival["rtDate"] | arrival["date"]; 
        String arrivalTimeStr = arrival["rtTime"] | arrival["time"]; 

        int arrivalHour = arrivalTimeStr.substring(0, 2).toInt();
        int arrivalMinute = arrivalTimeStr.substring(3, 5).toInt();
        int arrivalMinutes_calc = arrivalHour * 60 + arrivalMinute;

        int arrivalTotalMinutes = arrivalMinutes_calc;
        
        if (arrivalHour < currentHour && currentHour > 20 && arrivalHour < 5) { 
             arrivalTotalMinutes += 1440; 
        }

        if (arrivalTotalMinutes >= currentMinutes) {
          if (stopExtId == "1550" && arrivalTotalMinutes < nextTime1550_calc) {
            nextTime1550_calc = arrivalTotalMinutes;
            int diff = nextTime1550_calc - currentMinutes;
            nextArrival1550_display = "Next 1A in " + String(diff) + " min";
            arrivalTime1550_val = "(Arrives " + arrivalTimeStr + ")";
          } else if (stopExtId == "1583" && arrivalTotalMinutes < nextTime1583_calc) {
            nextTime1583_calc = arrivalTotalMinutes;
            int diff = nextTime1583_calc - currentMinutes;
            nextArrival1583_display = "Next 1A in " + String(diff) + " min";
            arrivalTime1583_val = "(Arrives " + arrivalTimeStr + ")";
          }
        }
      }
      Serial.println("Processed arrivals.");
      Serial.println(stopName1583 + ": " + nextArrival1583_display + " " + arrivalTime1583_val);
      Serial.println(stopName1550 + ": " + nextArrival1550_display + " " + arrivalTime1550_val);
    }
  } else {
    Serial.print("HTTP error: ");
    Serial.println(httpCode);
    nextArrival1550_display = "HTTP Error " + String(httpCode);
    nextArrival1583_display = "HTTP Error " + String(httpCode);
  }
  http.end();

  // Display formatted results on ePaper
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    
    int16_t current_y = 20; // Start Y position
    const int16_t line_height_12 = 20;
    const int16_t line_height_9 = 15;
    const int16_t left_margin = 5;
    const int16_t indent_margin = 15;

    // Title
    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(left_margin, current_y);
    display.print("Next Bus Arrivals (1A)");
    current_y += line_height_12 + 5; // Extra spacing after title

    // Switch to 9pt font for details
    display.setFont(&FreeMonoBold9pt7b);

    // Stop 1583 (Alhambravej)
    display.setCursor(left_margin, current_y);
    String shortStopName1583 = stopName1583;
    // Adjusted length for 400px width (approx 55-57 chars for 9pt font)
    if (shortStopName1583.length() > 55) shortStopName1583 = shortStopName1583.substring(0, 52) + "..."; 
    display.print("Stop: " + shortStopName1583);
    current_y += line_height_9;

    display.setCursor(indent_margin, current_y);
    display.print(nextArrival1583_display);
    current_y += line_height_9;
    if (arrivalTime1583_val != "") {
        display.setCursor(indent_margin, current_y);
        display.print(arrivalTime1583_val);
        current_y += line_height_9;
    }

    current_y += 10; // Extra space between stops

    // Stop 1550 (H.C. Ørsteds Vej)
    display.setCursor(left_margin, current_y);
    String shortStopName1550 = stopName1550;
    // Adjusted length for 400px width
    if (shortStopName1550.length() > 55) shortStopName1550 = shortStopName1550.substring(0, 52) + "...";
    display.print("Stop: " + shortStopName1550);
    current_y += line_height_9;

    display.setCursor(indent_margin, current_y);
    display.print(nextArrival1550_display);
    current_y += line_height_9;
     if (arrivalTime1550_val != "") {
        display.setCursor(indent_margin, current_y);
        display.print(arrivalTime1550_val);
        current_y += line_height_9;
    }

    // Display last update time
    // display.height() will be 300 with rotation(0)
    if (current_y > display.height() - (line_height_9 * 2) ) { 
        current_y = display.height() - line_height_9 - 5; 
    } else {
        current_y += line_height_9; 
    }
    display.setCursor(left_margin, current_y);
    char updateTimeStr[20];
    sprintf(updateTimeStr, "Updated: %02d:%02d", localTime->tm_hour, localTime->tm_min);
    display.print(updateTimeStr);

  } while (display.nextPage());

  Serial.println("Display updated. Waiting for next cycle.");
  delay(60000);
}