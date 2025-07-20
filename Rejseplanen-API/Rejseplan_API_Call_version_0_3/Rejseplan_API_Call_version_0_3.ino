#include <WiFi.h>
#include <HTTPClient.h>
#include <List.hpp>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>

//const char *ssid = "TP-Link_6639";
//const char *password = "17584722";

//UTC offset for denmark.
int offset = 3600;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "dk.pool.ntp.org", offset, 60000);

//const char *ssid = "Labitat (free)";
//const char *password = "labitatisawesome";
const char *ssid = "bornhack";
const char *password = "";

String Base_url = "https://www.rejseplanen.dk/api/";
String Api_key = "8c957bde-a5bf-41a2-8e6c-535eb4782fa6";
String Time_set = "";
String Date_set = "";
String Max_arriavels = "2";
String Request_url = "";
String Request_url_1583 = "";
String Request_url_1550 = "";

//Get current date
String Date_url = "https://timeapi.io/api/time/current/zone?timeZone=Europe%2FCopenhagen";

String payload = "";
List<String> data_hold;
List<String> data_hold2;
JsonDocument doc;
JsonDocument payloadjs;

JsonDocument filtertime;
JsonDocument filterrtTime;

HTTPClient http;

int error_data = 0;
int error_server = 0;

void setup() 
{
	filtertime["Arrival"][0]["time"] = true;
	filtertime["Arrival"][0]["rtTime"] = true;
	filtertime["Arrival"][0]["stopExtId"] = true;

	Serial.begin(115200);
	delay(1000);

	WiFi.mode(WIFI_STA); // Optional
	WiFi.begin(ssid, password);
	Serial.println("\nConnecting");

	while (WiFi.status() != WL_CONNECTED) 
	{
		Serial.print(".");
		delay(100);
	}
	Serial.println("\nConnected to the WiFi network");
	Serial.print("Local ESP32 IP: ");
	Serial.println(WiFi.localIP());
	timeClient.begin();
}

//Filter the data from rejsepan api and added to list
void Filter_data_rejseplan()
{
	String tempdata;
	tempdata = payload;
//	Serial.println(payload);
	deserializeJson(payloadjs, payload, DeserializationOption::Filter(filtertime));
	JsonArray data = payloadjs["Arrival"];
	for (JsonVariant item : data) 
	{
		String busid = item["stopExtId"];
		String bustimes = "";
		if(busid == "1550")
		{
			if(data_hold.getSize() == Max_arriavels.toInt())
			{
				data_hold.clear();
			}
			bustimes = String(item["time"]) + " : " + String(item["rtTime"]);
//			Serial.println(bustimes);
			data_hold.add(bustimes);
		}
		else if(busid == "1583")
		{
			if(data_hold2.getSize() == Max_arriavels.toInt())
			{
				data_hold2.clear();
			}
			bustimes = String(item["time"]) + " : " + String(item["rtTime"]);
//			Serial.println(bustimes);
			data_hold2.add(bustimes);
		}
	}
//	serializeJson(payloadjs, Serial);
//	Serial.println();
}

//for every update to time and date string needs to be rebuild
void build_rejseplan_string()
{
	Request_url_1583 = Base_url+"multiArrivalBoard?idList=1583&date="+Date_set+"&time="+Time_set+"&maxJourneys="+Max_arriavels+"&accessId="+Api_key+"&format=json";
	Request_url_1550 = Base_url+"multiArrivalBoard?idList=1550&date="+Date_set+"&time="+Time_set+"&maxJourneys="+Max_arriavels+"&accessId="+Api_key+"&format=json";
}

void format_time_url(String Date_Raw, int NTP_time)
{
	//Sets date and time for start and current day to get data and prevent error in api call.
	//This will be called ones per day to ensure correct date.
	int Split_index = Date_Raw.indexOf('T');
	Date_set = Date_Raw.substring(0,Split_index);
	Time_set = Date_Raw.substring(Split_index+1,Split_index+6);
	int timezone = Time_set.substring(0,2).toInt();
	//handles sommer/winter time.
	if(timezone != NTP_time)
	{
		if(timezone == 0 && NTP_time == 23)
		{
			offset+=3600;
		}
		else if(timezone == 23 && NTP_time == 0)
		{
			offset-=3600;
		}
		else if(NTP_time < timezone)
		{
			offset+=3600;
		}
		else if(NTP_time > timezone)
		{
			offset-=3600;
		}
		timeClient.setTimeOffset(offset);
		timeClient.update();
	}
}

void Url_get(String url)
{
	//clear payload before data added.
	payload = "";
	if(WiFi.status()== WL_CONNECTED)
	{
		Retry_call:
		// Your Domain name with URL path or IP address with path
		http.begin(url);
		http.setUserAgent("Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:78.0) Gecko/20100101 Firefox/78.0");
		// Send HTTP GET request
		int httpResponseCode = http.GET();
		
		if (httpResponseCode>0) 
		{
//			Serial.println(url);
//			Serial.print("HTTP Response code: ");
//			Serial.println(httpResponseCode);
			payload = http.getString();
			if(payload.length() == 0)
			{
				Serial.println("there is no data");
				error_data++;
			}
		}
		else 
		{
			Serial.println("Error code: "+String(httpResponseCode));
			Serial.println(payload);
			error_server++;
		}
		http.end();
	}
	else 
	{
		Serial.println("WiFi Disconnected");
	}
}

void run_during_boot_and_day_switch()
{
	//Date request ones per day.
	Url_get(Date_url);		
	deserializeJson(doc, payload);
	//Gets date with winter sommer time
	format_time_url(doc["dateTime"],timeClient.getHours());
	//Clear jason data, not needed until next day.
	payload = "";
	doc.clear();
}

bool Change_stop_place = true;

void loop()
{
	timeClient.update();
//	Serial.println(timeClient.getFormattedTime());
	if(Date_set == "")
	{
		run_during_boot_and_day_switch();
	}
	//will run ones every min.
	if(Time_set != timeClient.getFormattedTime().substring(0,5))
	{
		if(timeClient.getFormattedTime().substring(0,5) == "00:00")
		{
			Serial.println("day switch");
			Serial.println(Date_set);
			Serial.println(Time_set);
			Serial.println(Request_url_1550);
			run_during_boot_and_day_switch();
		}
		Time_set = timeClient.getFormattedTime().substring(0,5);
		if(Change_stop_place)
		{
			build_rejseplan_string();
			Url_get(Request_url_1550);
			Filter_data_rejseplan();
			Change_stop_place = false;
		}
		else
		{
			build_rejseplan_string();
			Url_get(Request_url_1583);
			Filter_data_rejseplan();
			Change_stop_place = true;
		}
		Serial.println();
		Serial.println(timeClient.getFormattedTime());
		//Stop place 1
		for(int i = 0; i < data_hold.getSize(); i++)
		{
			Serial.println(data_hold[i]);
		}
		//Stop place 2
		for(int i = 0; i < data_hold2.getSize(); i++)
		{
			Serial.println(data_hold2[i]);
		}
	}
	Serial.println("error data count: " + String(error_data));
	Serial.println("error server count: " + String(error_server));
	delay(10000);
}



