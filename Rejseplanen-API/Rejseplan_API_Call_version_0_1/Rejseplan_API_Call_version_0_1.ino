#include <WiFi.h>
#include <HTTPClient.h>

const char *ssid = "";
const char *password = "";

//const char *ssid = "";
//const char *password = "";

String Base_url = "https://www.rejseplanen.dk/api/";
String Api_key = "";
String Time_set = "12:10";
String Date_set = "2025-06-01";
String Id_stop = "1550";
String Request_url = Base_url+"multiArrivalBoard?idList="+Id_stop+"&date="+Date_set+"&time="+Time_set+"&accessId="+Api_key;
//String Request_url = Base_url+"multiArrivalBoard?idList=46676&date=2025-05-23&time=23:45&accessId="+Api_key;

String payload;
String holddata;
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
bool callone = true;

void loop()
{
	String tempdata;
	if(callone)
	{
		if(WiFi.status()== WL_CONNECTED)
		{
			HTTPClient http;
			String serverPath = Request_url;
			// Your Domain name with URL path or IP address with path
			http.begin(serverPath.c_str());
			// Send HTTP GET request
			int httpResponseCode = http.GET();
			
			if (httpResponseCode>0) 
			{
				Serial.print("HTTP Response code: ");
				Serial.println(httpResponseCode);
				payload = http.getString();
				tempdata = payload;
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
		Serial.println(payload);

		while(tempdata.indexOf("<Arrival ")>0)
		{
			int indexStart = tempdata.indexOf("<Arrival ");
			int indexStop = 0;
			for(int i = indexStart; i < tempdata.length(); i++)
			{
				if(tempdata[i] == '>')
				{
					indexStop = i+1;
					break;
				}
			}
			holddata = holddata+tempdata.substring(indexStart, indexStop);
			Serial.println(tempdata.substring(indexStart, indexStop));
			tempdata = tempdata.substring(indexStop, tempdata.length());
		}
		Serial.println(holddata);
		callone = false;
	}
}




