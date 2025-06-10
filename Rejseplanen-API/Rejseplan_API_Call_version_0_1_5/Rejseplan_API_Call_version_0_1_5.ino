#include <WiFi.h>
#include <HTTPClient.h>

const char *ssid = "";
const char *password = "";

String Base_url = "https://www.rejseplanen.dk/api/";
String Api_key = "";
String Time_set = "20:50";
String Date_set = "2025-06-07";
String Id_stop = "1550|1583";
String Max_arriavels = "6";
String Request_url = Base_url+"multiArrivalBoard?idList="+Id_stop+"&date="+Date_set+"&time="+Time_set+"&maxJourneys="+Max_arriavels+"&accessId="+Api_key+"&format=json";
//String Request_url = Base_url+"multiArrivalBoard?idList="+Id_stop+"&date="+Date_set+"&time="+Time_set+"&duration=30&accessId="+Api_key+"&format=json";
String payload = "";

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
}

//Filter the data from rejsepan api and added to list
void Filter_data_rejseplan(String data)
{
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
			Serial.println(payload);
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
bool runeones = true;
void loop()
{
	if (runeones)
	{
		Url_get(Request_url);
		Filter_data_rejseplan(payload);
		runeones = false;
	}
}



