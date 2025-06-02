#include <WiFi.h>
#include <HTTPClient.h>
#include <List.hpp>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>

const char *ssid = "";
const char *password = "";

//UTC offset for denmark.
int offset = 3600;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "dk.pool.ntp.org", offset, 60000);

//const char *ssid = "Labitat (free)";
//const char *password = "labitatisawesome";

String Base_url = "https://www.rejseplanen.dk/api/";
String Api_key = "";
String Time_set = "";
String Date_set = "";
const char* Id_stop = "1550|1583";
String Max_arriavels = "3";
String Request_url = "";

int list_limit = 3;

//Get current date
String Date_url = "https://timeapi.io/api/time/current/zone?timeZone=Europe%2FCopenhagen";

String payload = "";
List<String> data_hold;
List<String> data_hold2;
JsonDocument doc;

void setup() 
{
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
void Filter_data_rejseplan(String data)
{
//	data_hold.clear();
	list_limit = 0;
	while(data.indexOf("<Arrival ")>0)
	{
		String String_build_time = "";
		int indexStart = data.indexOf("<Arrival ");
		int indexStop = 0;
		int pointid = 0;
		for(int i = indexStart; i < data.length(); i++)
		{
			if(data[i] == '>')
			{
				indexStop = i+1;
				break;
			}
		}
		if(data.substring(indexStart, indexStop).indexOf("PROGNOSED") > 0)
		{
			pointid = data.substring(indexStart, indexStop).indexOf(" time=");
			String_build_time += data.substring(indexStart+pointid+7, indexStart+pointid+15);
			String_build_time += ",";
			pointid = data.substring(indexStart, indexStop).indexOf("rtTime=");
			String_build_time += data.substring(indexStart+pointid+8, indexStart+pointid+16);
		}
		else
		{
			pointid = data.substring(indexStart, indexStop).indexOf(" time=");
			String_build_time += data.substring(indexStart+pointid+7, indexStart+pointid+15);
		}
		if(data.substring(indexStart, indexStop).indexOf("stopExtId=\"1550\"") > 0 && data_hold.getSize() < 3)
		{
			data_hold.add(String_build_time);
		}
		if(data.substring(indexStart, indexStop).indexOf("stopExtId=\"1583\"") > 0 && data_hold.getSize() < 3)
		{
			data_hold2.add(String_build_time);
		}
		Serial.println(String_build_time);
		data = data.substring(indexStop, data.length());
/*		list_limit++;
		if(list_limit == 3)
			break;*/
	}
}

//for every update to time and date string needs to be rebuild
void build_rejseplan_string()
{
//	Request_url = Base_url+"multiArrivalBoard?idList="+Id_stop+"&date="+Date_set+"&time="+Time_set+"&maxJourneys="+Max_arriavels+"&accessId="+Api_key;
	Request_url = Base_url+"multiArrivalBoard?idList="+String(Id_stop)+"&date="+Date_set+"&time="+Time_set+"&accessId="+Api_key+ "&format=json";
//	Request_url = Base_url+"multiArrivalBoard?idList="+Id_stop+"&date="+Date_set+"&time="+Time_set+"&accessId="+Api_key;
//	Request_url = Base_url+"multiArrivalBoard?idList=1550&date="+Date_set+"&time="+Time_set+"&accessId="+Api_key;
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
		HTTPClient http;
		String serverPath = url;
		// Your Domain name with URL path or IP address with path
		http.begin(serverPath);
		// Send HTTP GET request
		int httpResponseCode = http.GET();
		
		if (httpResponseCode>0) 
		{
			Serial.println(url);
			Serial.print("HTTP Response code: ");
			Serial.println(httpResponseCode);
			payload = http.getString();
			if(payload.length() > 2)
			{
				Serial.println("there is data");
			}
			else
			{
				Serial.println("there is no data");
			}
		}
		else 
		{
			Serial.print("Error code: ");
			Serial.println(httpResponseCode);
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
	doc.clear();
}
void loop()
{
	timeClient.update();
	Serial.println(timeClient.getFormattedTime());
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
			Serial.println(Request_url);
			run_during_boot_and_day_switch();
		}
		Time_set = timeClient.getFormattedTime().substring(0,5);
		build_rejseplan_string();
		delay(10);
		Url_get(Request_url);
		Filter_data_rejseplan(payload);
		for(int i = 0; i < data_hold.getSize(); i++)
		{
			Serial.println(data_hold[i]);
		}
//		data_hold.clear();
	}
	delay(10000);
}



