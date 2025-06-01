// Define a higher nesting limit to handle deep JSON structures
#define ARDUINOJSON_DEFAULT_NESTING_LIMIT 16

// Include necessary libraries
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

// WiFi credentials (replace with your own)
const char* ssid = "Yunes";
const char* password = "123456789";

// API details
const char* apiKey = "9b00b65e-e873-45af-8ff8-47366a137f53";
const char* stopIds = "1550|1583";

// Filter for JSON parsing to optimize memory usage
StaticJsonDocument<256> filter;

void setup() {
  // Initialize serial communication
  Serial.begin(115200);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // Set time via NTP (UTC+2 for CEST)
  configTime(7200, 0, "pool.ntp.org");

  // Wait for time to synchronize
  while (time(nullptr) < 100000) {
    delay(1000);
    Serial.println("Waiting for time sync...");
  }

  // Define the filter for JSON parsing
  JsonObject arrivalFilter = filter["Arrival"].createNestedObject();
  arrivalFilter["stopExtId"] = true;  // Stop ID
  arrivalFilter["name"] = true;       // Line name
  arrivalFilter["time"] = true;       // Scheduled time
  arrivalFilter["date"] = true;       // Scheduled date
  arrivalFilter["rtTime"] = true;     // Real-time time (if available)
  arrivalFilter["rtDate"] = true;     // Real-time date (if available)
}

void loop() {
  // Get current time
  time_t now = time(nullptr);
  struct tm *localTime = localtime(&now);

  // Extract current date and time
  char currentDate[11];
  strftime(currentDate, sizeof(currentDate), "%Y-%m-%d", localTime);
  char currentTime[6];
  strftime(currentTime, sizeof(currentTime), "%H:%M", localTime);
  int currentHour = localTime->tm_hour;
  int currentMinute = localTime->tm_min;
  int currentMinutes = currentHour * 60 + currentMinute;

  // Construct API URL
  String url = "https://www.rejseplanen.dk/api/multiArrivalBoard?idList=" + String(stopIds) +
               "&date=" + currentDate + "&time=" + currentTime + "&accessId=" + apiKey + "&format=json";

  // Make HTTP request
  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();

    // Parse JSON with filter and increased document size
    StaticJsonDocument<4096> doc;
    DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
    if (error) {
      Serial.print("JSON parsing error: ");
      Serial.println(error.c_str());
      return;
    }

    // Process arrivals
    JsonArray arrivals = doc["Arrival"];

    // Initialize variables for next arrivals (use a large value to detect no arrivals)
    int nextTime1550 = 1440 * 2;  // Max minutes for two days
    String nextArrival1550;
    int nextTime1583 = 1440 * 2;
    String nextArrival1583;

    // Define stop names
    String stopName1550 = "Gammel Kongevej (H.C. Ørsteds Vej)";
    String stopName1583 = "Gammel Kongevej (Alhambravej)";

    for (JsonObject arrival : arrivals) {
      String stopExtId = arrival["stopExtId"].as<String>();
      String line = arrival["name"].as<String>();

      // Only process arrivals for "Bus 1A"
      if (line != "Bus 1A") continue;

      String arrivalDate = arrival["rtDate"] | arrival["date"];
      String arrivalTime = arrival["rtTime"] | arrival["time"];

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
        } else if (stopExtId == "1583" && arrivalTotalMinutes < nextTime1583) {
          nextTime1583 = arrivalTotalMinutes;
          nextArrival1583 = arrivalDate + " " + arrivalTime;
        }
      }
    }

    // Display formatted results
    Serial.println("────────── Next Bus Arrivals ──────────");

    // Stop 1583
    Serial.print("Stop: ");
    Serial.println(stopName1583);
    if (nextTime1583 < 1440 * 2) {
      int diff = nextTime1583 - currentMinutes;
      Serial.print("  Next bus (line 1A) in ");
      Serial.print(diff);
      Serial.println(" minutes");
      Serial.print("    (Arrives ");
      Serial.print(nextArrival1583);
      Serial.println(")");
    } else {
      Serial.println("  No more buses today");
    }

    // Stop 1550
    Serial.print("Stop: ");
    Serial.println(stopName1550);
    if (nextTime1550 < 1440 * 2) {
      int diff = nextTime1550 - currentMinutes;
      Serial.print("  Next bus (line 1A) in ");
      Serial.print(diff);
      Serial.println(" minutes");
      Serial.print("    (Arrives ");
      Serial.print(nextArrival1550);
      Serial.println(")");
    } else {
      Serial.println("  No more buses today");
    }
  } else {
    Serial.print("HTTP error: ");
    Serial.println(httpCode);
  }

  http.end();
  delay(60000); // Update every minute
}