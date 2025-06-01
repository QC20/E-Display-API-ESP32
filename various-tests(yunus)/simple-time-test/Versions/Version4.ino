#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "time.h"

// WiFi credentials
const char* ssid = "Yunes";
const char* password = "123456789";

// API details
const char* apiKey = "9b00b65e-e873-45af-8ff8-47366a137f53";
const char* stopIds = "1550|1583";

// NTP server settings
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7200;  // UTC+2 for CEST
const int   daylightOffset_sec = 0;

// Time structure
struct TimeData {
  int hours;
  int minutes;
  int seconds;
};

// Filter for JSON parsing to optimize memory usage
StaticJsonDocument<256> filter;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Connect to WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Configure NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  delay(1000);  // Give time to sync
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Initial NTP sync failed, retrying in loop...");
  }

  // Define the filter for JSON parsing
  JsonObject arrivalFilter = filter["Arrival"].createNestedObject();
  arrivalFilter["stopExtId"] = true;  // Stop ID
  arrivalFilter["name"] = true;       // Line name
  arrivalFilter["time"] = true;       // Scheduled time
  arrivalFilter["date"] = true;       // Scheduled date
  arrivalFilter["rtTime"] = true;     // Real-time time
  arrivalFilter["rtDate"] = true;     // Real-time date
}

void loop() {
  static int lastMinute = -1;
  TimeData currentTime = getCurrentTimeFromNTP();

  // Process only if time is valid and the minute has changed
  if (currentTime.hours != -1 && currentTime.minutes != lastMinute) {
    lastMinute = currentTime.minutes;

    // Display time information
    Serial.println("=== Time Info ===");
    Serial.print("Current time: ");
    printTime(currentTime);

    // Fetch and process bus arrivals
    checkBusArrivals(currentTime);

    Serial.println("=================");
  }

  delay(100);  // Check 10 times per second to catch minute changes
}

// Get current time from NTP with error handling
TimeData getCurrentTimeFromNTP() {
  TimeData timeData = {-1, -1, -1};
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time, attempting to resync...");
    maintainWiFi();  // Ensure WiFi is connected
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    delay(1000);  // Wait a second for sync
    if (!getLocalTime(&timeinfo)) {
      Serial.println("Still failed to obtain time.");
      return timeData;  // Return invalid time if sync fails
    }
  }

  timeData.hours = timeinfo.tm_hour;
  timeData.minutes = timeinfo.tm_min;
  timeData.seconds = timeinfo.tm_sec;

  return timeData;
}

// Maintain WiFi connection
void maintainWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, attempting to reconnect...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("WiFi reconnected.");
  }
}

// Print time in HH:MM:SS format
void printTime(TimeData time) {
  if (time.hours < 10) Serial.print("0");
  Serial.print(time.hours);
  Serial.print(":");
  if (time.minutes < 10) Serial.print("0");
  Serial.print(time.minutes);
  Serial.print(":");
  if (time.seconds < 10) Serial.print("0");
  Serial.println(time.seconds);
}

// Check bus arrivals from Rejseplanen API and calculate countdown
void checkBusArrivals(TimeData currentTime) {
  // Extract current date and time for API
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Cannot fetch bus arrivals: no valid time");
    return;
  }
  char currentDate[11];
  strftime(currentDate, sizeof(currentDate), "%Y-%m-%d", &timeinfo);
  char currentTimeStr[6];
  strftime(currentTimeStr, sizeof(currentTimeStr), "%H:%M", &timeinfo);
  int currentMinutes = currentTime.hours * 60 + currentTime.minutes;

  // Construct API URL
  String url = "https://www.rejseplanen.dk/api/multiArrivalBoard?idList=" + String(stopIds) +
               "&date=" + currentDate + "&time=" + currentTimeStr + "&accessId=" + apiKey + "&format=json";

  // Debug: Log API interaction and network details
  Serial.println("--- API Debug Info ---");
  Serial.print("API URL: ");
  Serial.println(url);
  Serial.print("WiFi signal strength (RSSI): ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");

  // Make HTTP request
  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();

  Serial.print("HTTP response code: ");
  Serial.println(httpCode);

  if (httpCode == 200) {
    String payload = http.getString();
    Serial.print("Payload size (bytes): ");
    Serial.println(payload.length());
    Serial.print("Raw JSON snippet (first 100 chars): ");
    Serial.println(payload.substring(0, 100));

    // Set memory for JSON parsing
    // This line sets the memory size to 4096 bytes, which is enough for most bus arrival data from the API
    StaticJsonDocument<4096> doc;
    // OPTION 1: Use 2048 bytes instead
    // Why: Saves memory on devices with less space (e.g., Arduino Uno), but might fail if the API sends lots of data
    // How to test: Uncomment this, upload the code, and check Serial for "JSON parsing error"
    // StaticJsonDocument<2048> doc;
    // OPTION 2: Use 1024 bytes for very low memory
    // Why: Good for tiny devices, but very likely to fail if many bus arrivals are in the response
    // How to test: Uncomment this, watch Serial for errors, and increase size if needed
    // StaticJsonDocument<1024> doc;

    // Parse JSON with filter
    DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
    if (error) {
      Serial.print("JSON parsing error: ");
      Serial.println(error.c_str());
      http.end();
      return;
    }

    // Process arrivals
    JsonArray arrivals = doc["Arrival"];
    Serial.print("Number of arrivals processed: ");
    Serial.println(arrivals.size());

    // Initialize variables for next arrivals
    int nextTime1550 = 1440 * 2;  // Max minutes for two days
    String nextArrival1550;
    bool hasRealTime1550 = false;
    int nextTime1583 = 1440 * 2;
    String nextArrival1583;
    bool hasRealTime1583 = false;

    // Define stop names
    String stopName1550 = "Gammel Kongevej (H.C. Ørsteds Vej)";
    String stopName1583 = "Gammel Kongevej (Alhambravej)";

    for (JsonObject arrival : arrivals) {
      String stopExtId = arrival["stopExtId"].as<String>();
      String line = arrival["name"].as<String>();

      // Only process arrivals for "Bus 1A"
      if (line != "Bus 1A") continue;

      // Check for real-time data availability
      bool hasRealTime = arrival["rtTime"].is<String>() && arrival["rtDate"].is<String>();
      String arrivalDate = hasRealTime ? arrival["rtDate"].as<String>() : arrival["date"].as<String>();
      String arrivalTime = hasRealTime ? arrival["rtTime"].as<String>() : arrival["time"].as<String>();

      // Convert arrival time to minutes since midnight
      int arrivalHour = arrivalTime.substring(0, 2).toInt();
      int arrivalMinute = arrivalTime.substring(3, 5).toInt();
      int arrivalMinutes = arrivalHour * 60 + arrivalMinute;

      // Adjust for next day if necessary
      int arrivalTotalMinutes = arrivalMinutes;
      if (arrivalDate != String(currentDate)) {
        arrivalTotalMinutes += 1440;  // Add a day's worth of minutes
      }

      // Find the earliest arrival after current time
      if (arrivalTotalMinutes >= currentMinutes) {
        if (stopExtId == "1550" && arrivalTotalMinutes < nextTime1550) {
          nextTime1550 = arrivalTotalMinutes;
          nextArrival1550 = arrivalDate + " " + arrivalTime;
          hasRealTime1550 = hasRealTime;
        } else if (stopExtId == "1583" && arrivalTotalMinutes < nextTime1583) {
          nextTime1583 = arrivalTotalMinutes;
          nextArrival1583 = arrivalDate + " " + arrivalTime;
          hasRealTime1583 = hasRealTime;
        }
      }
    }

    // Display formatted results in a clear window
    Serial.println("────────── Next Bus Arrivals ──────────");

    // Stop 1583
    Serial.print("Bus stoppested: ");
    Serial.println(stopName1583);
    int diff1583 = nextTime1583 - currentMinutes;
    if (nextTime1583 < 1440 * 2 && diff1583 <= 60 && diff1583 >= 0) {
      if (diff1583 == 0) {
        Serial.println("  Next bus (line 1A) is arriving now!");
      } else {
        Serial.print("  Next bus (line 1A) will be here in +");
        Serial.print(diff1583);
        Serial.println(" minutes");
      }
      Serial.print("    (Arrives ");
      Serial.print(nextArrival1583);
      Serial.print(hasRealTime1583 ? " - real-time estimate)" : " - scheduled estimate)");
      Serial.println();
    } else if (nextTime1583 >= 1440 * 2 || diff1583 > 60) {
      Serial.println("  Ingen busser ankommer indenfor den næste time.");
    } else {
      Serial.println("  Next bus (line 1A) may have just passed, checking next...");
    }

    // Stop 1550
    Serial.print("Bus stoppested: ");
    Serial.println(stopName1550);
    int diff1550 = nextTime1550 - currentMinutes;
    if (nextTime1550 < 1440 * 2 && diff1550 <= 60 && diff1550 >= 0) {
      if (diff1550 == 0) {
        Serial.println("  Next bus (line 1A) is arriving now!");
      } else {
        Serial.print("  Next bus (line 1A) will be here in +");
        Serial.print(diff1550);
        Serial.println(" minutes");
      }
      Serial.print("    (Arrives ");
      Serial.print(nextArrival1550);
      Serial.print(hasRealTime1550 ? " - real-time estimate)" : " - scheduled estimate)");
      Serial.println();
    } else if (nextTime1550 >= 1440 * 2 || diff1550 > 60) {
      Serial.println("  Ingen busser ankommer indenfor den næste time.");
    } else {
      Serial.println("  Next bus (line 1A) may have just passed, checking next...");
    }
    Serial.println("───────────────────────────────────────");
  } else {
    Serial.print("HTTP error: ");
    Serial.println(httpCode);
  }

  http.end();
}