// Define a higher nesting limit to handle deep JSON structures
#define ARDUINOJSON_DEFAULT_NESTING_LIMIT 16

// ------------------------------------------------------------------------------------
// WIRING INSTRUCTIONS: Arduino Nano ESP32 to WeAct 4.2" Epaper
// (Using GPIO numbers from your working setup)
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
// ePaper DC   -> Connect to the pin corresponding to GPIO0 on your Nano ESP32
// ePaper RST  -> Connect to the pin corresponding to GPIO2 on your Nano ESP32
// ePaper BUSY -> Connect to the pin corresponding to GPIO15 on your Nano ESP32
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
#include <Fonts/FreeMonoBold18pt7b.h> // For larger titles

// Initialize the display for the 4.2" ePaper module using GPIO numbers
GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT> display(GxEPD2_420_GDEY042T81(
  /*CS=*/   5,    // Chip Select pin (GPIO5)
  /*DC=*/   0,    // Data/Command pin (GPIO0)
  /*RST=*/  2,    // Reset pin (GPIO2)
  /*BUSY=*/ 15    // Busy pin (GPIO15)
));
// ------------------------------------------------------------------------------------

// WiFi credentials
const char* ssid = "Yunes";
const char* password = "123456789";

// API details
const char* apiKey = "9b00b65e-e873-45af-8ff8-47366a137f53";
const char* stopIds = "1550|1583";

StaticJsonDocument<256> filter;
String stopName1550 = "Gammel Kongevej (H.C. Ørsteds Vej)";
String stopName1583 = "Gammel Kongevej (Alhambravej)";

// Helper function to draw information for a single stop
void drawStopInfo(int16_t &y_current_top, // Pass by reference to update Y for the next element
                  const String& mainName, const String& subName, 
                  const String& minsToArrivalStr, const String& arrivalTimeStr_val, 
                  const String& noServiceMsg, 
                  int16_t l_margin, int16_t usable_width) {
    
    int16_t tbx, tby; uint16_t tbw, tbh; // For getTextBounds results

    // --- Left side: Stop Name ---
    display.setFont(&FreeMonoBold12pt7b);
    display.getTextBounds(mainName, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setCursor(l_margin, y_current_top);
    display.print(mainName);
    int16_t height_main_name = tbh;

    display.setFont(&FreeMonoBold9pt7b);
    display.getTextBounds(subName, 0, 0, &tbx, &tby, &tbw, &tbh);
    int16_t sub_name_y_offset = height_main_name + 8; // 8px spacing below main name
    display.setCursor(l_margin + 10, y_current_top + sub_name_y_offset); // Indent sub-name
    display.print(subName);
    int16_t height_sub_name = tbh;
    
    int16_t total_height_left_text_block = sub_name_y_offset + height_sub_name;

    // --- Right side: Arrival Info ---
    int16_t arrival_info_right_x_edge = usable_width - l_margin; // X for right alignment target

    int16_t height_arrival_line1 = 0;
    int16_t height_arrival_line2 = 0;

    if (!minsToArrivalStr.isEmpty()) { // If there's an upcoming bus
        display.setFont(&FreeMonoBold18pt7b); // Large font for "X min"
        display.getTextBounds(minsToArrivalStr, 0, 0, &tbx, &tby, &tbw, &tbh);
        display.setCursor(arrival_info_right_x_edge - tbw, y_current_top); // Align top with mainName's top
        display.print(minsToArrivalStr);
        height_arrival_line1 = tbh;

        display.setFont(&FreeMonoBold9pt7b);
        display.getTextBounds(arrivalTimeStr_val, 0, 0, &tbx, &tby, &tbw, &tbh);
        int16_t arrival_time_y_offset = height_arrival_line1 + 6; // 6px spacing
        display.setCursor(arrival_info_right_x_edge - tbw, y_current_top + arrival_time_y_offset);
        display.print(arrivalTimeStr_val);
        height_arrival_line2 = tbh;
    } else { // "No service" or "Error" message
        display.setFont(&FreeMonoBold12pt7b); // Use 12pt for this message
        display.getTextBounds(noServiceMsg, 0, 0, &tbx, &tby, &tbw, &tbh);
        // Vertically center the "No service" message roughly with the two lines of the stop name
        int16_t no_service_y = y_current_top + (total_height_left_text_block / 2) - (tbh / 2);
        display.setCursor(arrival_info_right_x_edge - tbw, no_service_y);
        display.print(noServiceMsg);
        height_arrival_line1 = tbh; // Effectively one line for this info
    }
    int16_t total_height_right_text_block = height_arrival_line1 + (height_arrival_line2 > 0 ? (6 + height_arrival_line2) : 0);
    
    // Update y_current_top to be below the tallest part of this block for the *next* element
    y_current_top += max(total_height_left_text_block, total_height_right_text_block);
}


void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("Starting E-Paper Bus Schedule Display (More Spacing)...");

  display.init(115200, true, 10, false);
  display.setRotation(0); 

  display.setFont(&FreeMonoBold12pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(15, 30); // Increased left margin for init text
    display.print("Initializing...");
  } while (display.nextPage());

  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  int wifi_retries = 0;
  while (WiFi.status() != WL_CONNECTED && wifi_retries < 30) {
    delay(1000); Serial.print("."); wifi_retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" Connected!"); // Other init messages are similar...
  } else {
    Serial.println(" WiFi Connection Failed!"); /* ... */ while(true) delay(1000);
  }

  configTime(7200, 0, "pool.ntp.org");
  time_t now_check = time(nullptr);
  while (now_check < 100000) {
    delay(1000); Serial.println("Waiting for time sync..."); now_check = time(nullptr);
  }
  Serial.println("Time synchronized.");

  JsonObject arrivalFilter = filter["Arrival"].createNestedObject();
  arrivalFilter["stopExtId"] = true; arrivalFilter["name"] = true;
  arrivalFilter["time"] = true; arrivalFilter["date"] = true;
  arrivalFilter["rtTime"] = true; arrivalFilter["rtDate"] = true;
  Serial.println("Setup complete.");
}

void loop() {
  Serial.println("Fetching new bus data...");

  time_t now = time(nullptr);
  struct tm *localTime = localtime(&now);
  char currentDate[11]; strftime(currentDate, sizeof(currentDate), "%Y-%m-%d", localTime);
  char currentTime[6]; strftime(currentTime, sizeof(currentTime), "%H:%M", localTime);
  int currentHour = localTime->tm_hour; int currentMinute = localTime->tm_min;
  int currentMinutes = currentHour * 60 + currentMinute;

  String url = "https://www.rejseplanen.dk/api/multiArrivalBoard?idList=" + String(stopIds) +
               "&date=" + currentDate + "&time=" + currentTime + "&accessId=" + apiKey + "&format=json";

  String nextArrival1550_msg_main = ""; // Will hold "Now" or "X min"
  String nextArrival1583_msg_main = "";
  String arrivalTime1550_val = "";
  String arrivalTime1583_val = "";
  String errorOrNoService1550 = "No service"; // Default if no bus / error
  String errorOrNoService1583 = "No service";

  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    StaticJsonDocument<4096> doc;
    DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));

    if (error) {
      Serial.print("JSON parsing error: "); Serial.println(error.c_str());
      errorOrNoService1550 = "Data error"; errorOrNoService1583 = "Data error";
    } else {
      JsonArray arrivals = doc["Arrival"];
      int nextTime1550_calc = 1440 * 2 + 1; int nextTime1583_calc = 1440 * 2 + 1;

      for (JsonObject arrival : arrivals) {
        String stopExtId = arrival["stopExtId"].as<String>();
        if ((arrival["name"].as<String>()) != "Bus 1A") continue;

        String arrivalTimeStr = arrival["rtTime"] | arrival["time"];
        int arrivalHour = arrivalTimeStr.substring(0, 2).toInt();
        int arrivalMinutes_calc = (arrivalHour * 60) + arrivalTimeStr.substring(3, 5).toInt();
        int arrivalTotalMinutes = arrivalMinutes_calc;
        if (arrivalHour < currentHour && currentHour > 20 && arrivalHour < 5) arrivalTotalMinutes += 1440;

        if (arrivalTotalMinutes >= currentMinutes) {
          int diff = arrivalTotalMinutes - currentMinutes;
          if (stopExtId == "1550" && arrivalTotalMinutes < nextTime1550_calc) {
            nextTime1550_calc = arrivalTotalMinutes;
            nextArrival1550_msg_main = (diff == 0) ? "Now" : String(diff) + " min";
            arrivalTime1550_val = arrivalTimeStr;
          } else if (stopExtId == "1583" && arrivalTotalMinutes < nextTime1583_calc) {
            nextTime1583_calc = arrivalTotalMinutes;
            nextArrival1583_msg_main = (diff == 0) ? "Now" : String(diff) + " min";
            arrivalTime1583_val = arrivalTimeStr;
          }
        }
      }
      if (nextArrival1550_msg_main.isEmpty()) errorOrNoService1550 = "No service"; // Re-affirm if nothing found
      if (nextArrival1583_msg_main.isEmpty()) errorOrNoService1583 = "No service";
    }
  } else {
    Serial.print("HTTP error: "); Serial.println(httpCode);
    String httpErrorMsg = "Error " + String(httpCode);
    errorOrNoService1550 = httpErrorMsg; errorOrNoService1583 = httpErrorMsg;
  }
  http.end();

  // --- Display Drawing Logic ---
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    
    int16_t current_y = 0;
    int16_t left_margin = 15; // Increased overall left margin
    int16_t usable_width = display.width(); // Should be 400

    int16_t tbx, tby; uint16_t tbw, tbh; // For getTextBounds

    // --- Header ---
    current_y = 30; // Top padding for the entire display content
    
    display.setFont(&FreeMonoBold18pt7b);
    display.getTextBounds("BUS 1A", 0, 0, &tbx, &tby, &tbw, &tbh);
    int16_t header_line_height = tbh; // Height of the "BUS 1A" text
    display.setCursor(left_margin, current_y);
    display.print("BUS 1A");

    String header_text_right = "Next Arrivals";
    display.setFont(&FreeMonoBold12pt7b); 
    display.getTextBounds(header_text_right, 0, 0, &tbx, &tby, &tbw, &tbh);
    // Align top of "Next Arrivals" with top of "BUS 1A"
    display.setCursor(usable_width - left_margin - tbw, current_y);
    display.print(header_text_right);
    
    current_y += header_line_height + 12; // Space after header text (increased to 12)
    display.drawFastHLine(left_margin, current_y, usable_width - (2*left_margin), GxEPD_BLACK);
    current_y += 20; // Generous space after header line (increased to 20)

    // --- Stop 1 (Alhambravej - 1583) ---
    // Pass appropriate messages to drawStopInfo
    drawStopInfo(current_y, "Gammel Kongevej", "(Alhambravej)", 
                 nextArrival1583_msg_main, arrivalTime1583_val, errorOrNoService1583, 
                 left_margin, usable_width);
    
    current_y += 18; // Space after stop 1 content block (increased to 18)
    display.drawFastHLine(left_margin + 15, current_y, usable_width - (2*(left_margin+15)), GxEPD_BLACK); // Line between stops
    current_y += 18; // Space after the line, before next stop (increased to 18)

    // --- Stop 2 (H.C. Ørsteds Vej - 1550) ---
    drawStopInfo(current_y, "Gammel Kongevej", "(H.C. Ørsteds Vej)", 
                 nextArrival1550_msg_main, arrivalTime1550_val, errorOrNoService1550, 
                 left_margin, usable_width);

    // --- Footer: Updated time ---
    display.setFont(&FreeMonoBold9pt7b);
    char updateTimeStr[20];
    sprintf(updateTimeStr, "Updated: %02d:%02d", localTime->tm_hour, localTime->tm_min);
    display.getTextBounds(updateTimeStr, 0, 0, &tbx, &tby, &tbw, &tbh);
    // Ensure it's not drawn off-screen if content is too tall
    int16_t footer_y = display.height() - tbh - 15; // 15px from bottom
    if (current_y + 20 > footer_y) { // If current content is pushing too low
         // Could potentially overlap, but usually there's enough space on 400x300
    }
    display.setCursor(usable_width - left_margin - tbw, footer_y); 
    display.print(updateTimeStr);

  } while (display.nextPage());

  Serial.println("Display updated with more spacing. Waiting for next cycle.");
  delay(60000); 
}