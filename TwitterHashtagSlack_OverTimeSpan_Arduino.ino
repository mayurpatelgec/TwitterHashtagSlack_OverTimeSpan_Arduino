
/*********************************************************
Author: Mayur Patel
Email : mayurpatel.gec@gmail.com
Date  : 07/10/2018
Descr.: Arduino compatible code to track a #xxxxxxx hashtag
        on twiiter over a certain timespan and notification 
        on Slack channel via twiiter app, slack app & 
        slack_webhook
Ex.Use: tracks #hashtag in twitts between 07:00 to 07:59 hrs 
        and 17:00 to 17:59 hrs
**********************************************************/
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <ArduinoJson.h>                  
#include <TwitterWebAPI.h>


/*/////////////CONFIGURATION/////////////////////////////
/*
 WiFi
 */
#ifndef WIFICONFIG
const char* ssid = "xxxxxxxxxx";           // WiFi SSID
const char* password = "xxxxxxxxxxxx";   // WiFi Password
#endif

/*
 SLACK
 */
const String slack_hook_url = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
const String slack_icon_url = "<SLACK_ICON_URL>"; // change if u need
const String slack_username = "<SLACK_USERNAME>"; // change if u need

/*
 Time
 */
const char *ntp_server = "pool.ntp.org";  // time1.google.com, time.nist.gov, pool.ntp.org
int timezone = 0;                        // UTC timezone 0000:00 HRS // dont change bcz twitt time is alwys UTC+0000
unsigned long twi_update_interval = 20;   // (seconds) minimum 5s (180 API calls/15 min). Any value less than 5 is ignored!

/*
SEARCH TIME
*/
int TIME_OFFSET_HR=2;       // Germany UTC + 2
int REQ_TIME1=17;           // 17:00 to 17:59 Hrs (UTC + 2)
int REQ_TIME2=07;           // 07:00 to 07:59 Hrs (UTC + 2)
/*
 TWITTER
 */
#ifndef TWITTERINFO  // Obtain these by creating an app @ https://apps.twitter.com/
  static char const consumer_key[]    = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
  static char const consumer_sec[]    = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
  static char const accesstoken[]     = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
  static char const accesstoken_sec[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
#endif
std::string search_str = "#xxxxxxx";          // Default search word for twitter
/*////////////////////////////////////////////*/

//   Dont change anything below this line    //



/*/////////////////////////////////////////////*/
// Global Variables:
unsigned long api_mtbs = twi_update_interval * 1000; //mean time between api requests
unsigned long api_lasttime = 0; 
bool twit_update = false;
std::string search_msg = "No Message Yet!";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntp_server, timezone*3600, 60000);  // NTP server pool, offset (in seconds), update interval (in milliseconds)
TwitterClient tcr(timeClient, consumer_key, consumer_sec, accesstoken, accesstoken_sec);

String twitid_old,twitid_new;
/*/////////////////////////////////////////////*/
void setup(void){
  //Begin Serial
  Serial.begin(115200);
  // WiFi Connection
  WiFi.begin(ssid, password);
  Serial.print("\nConnecting to ");
  Serial.print(ssid);
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected. yay!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  delay(100);
  // Connect to NTP and force-update time
  tcr.startNTP();
  Serial.println("NTP Synced");
  delay(100);
  // Setup internal LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  if (twi_update_interval < 5) api_mtbs = 5000; // Cant update faster than 5s.
}

bool postMessageToSlack(String msg)
{
  const char* host = "hooks.slack.com";
  Serial.print("Connecting to ");
  Serial.println(host);

  // Use WiFiClient class to create TCP connections
  WiFiClientSecure client;
  const int httpsPort = 443;
  if (!client.connect(host, httpsPort)) {
    Serial.println("Connection failed :-(");
    return false;
  }

  // We now create a URI for the request

  Serial.print("Posting to URL: ");
  Serial.println(slack_hook_url);

  String postData="payload={\"link_names\": 1, \"icon_url\": \"" + slack_icon_url + "\", \"username\": \"" + slack_username + "\", \"text\": \"" + msg + "\"}";

  // This will send the request to the server
  client.print(String("POST ") + slack_hook_url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Content-Type: application/x-www-form-urlencoded\r\n" +
               "Connection: close" + "\r\n" +
               "Content-Length:" + postData.length() + "\r\n" +
               "\r\n" + postData);
  Serial.println("Request sent");
  String line = client.readStringUntil('\n');
  Serial.printf("Response code was: ");
  Serial.println(line);
  if (line.startsWith("HTTP/1.1 200 OK")) {
    return true;
  } else {
    return false;
  }
}

void extractJSON(String tmsg) {
  const char* msg2 = const_cast <char*> (tmsg.c_str());
  DynamicJsonBuffer jsonBuffer;
  JsonObject& response = jsonBuffer.parseObject(msg2);
  Serial.println(msg2);
  if (!response.success()) {
    Serial.println("Failed to parse JSON!");
    Serial.println(msg2);
//    jsonBuffer.clear();
    return;
  }

  
  if (response.containsKey("statuses")) {
    // Twitter Response parsing of JSON
    String usert = response["statuses"][0]["user"]["screen_name"];  // Lasttwitt_username
    String text = response["statuses"][0]["text"];                  // Lasttwitt_Text
    String twitid_new = response["statuses"][0]["id"];              // Lasttwitt_Unique_TwittId
    String twittime = response["statuses"][0]["created_at"];        // Lasttwitt_CreationLongTime(UTC+0000) ex. Sun Oct 07 13:09:26 +0000 2018
    String onlytwittime = twittime.substring(11, 19);               // Lasttwitt_OnlyTime ex. 13:09:26
    String onlytwittime_hr = onlytwittime.substring(0,2);           // Lasttwitt_OnlyTime_HRs ex. 13 (Str)
    int currenttime_hr= timeClient.getHours();                      // CurrentTime_HRs ex. 14 (Int)
    
    if (twitid_old != twitid_new) //new twit
      { twitid_old = twitid_new;
        Serial.println("NEW_TWITT");
        twitid_new ="";
        Serial.println("onlytwittime_hr:");Serial.println(onlytwittime_hr.toInt());
        Serial.println("currenttime_hr:");Serial.println(currenttime_hr);

        if ( (onlytwittime_hr.toInt() == (REQ_TIME1-TIME_OFFSET_HR)) || (onlytwittime_hr.toInt() == (REQ_TIME2-TIME_OFFSET_HR)) )// check in required time
          { // in given time span
            Serial.println("SEND A NOTIFICATION!!!");
            String slack_message = "TRAIN LATE!!! \n TIME: " + twittime + "Please add 2 HRS in time " + "\n" + "TWIT:" + "\n" + "@" + usert + "\n" + text ;
            bool ok = postMessageToSlack(slack_message);
          }      
        else
        {
         //outof time span
         Serial.println("DONT SEND A NOTIFICATION!!!");
        }
      }
    else
      {// old twitt no update
        Serial.println("OLD_TWIT");
        }
        
    if (text != "") {
      text = "@" + usert + " says " + text + " twitid: " + twitid_new + "twittime:" + twittime ;
      search_msg = std::string(text.c_str(), text.length());
    }
   
  } else if(response.containsKey("errors")) {
    String err = response["errors"][0];
    search_msg = std::string(err.c_str(), err.length());
  } else {
    Serial.println("No useful data");
  }
  
  jsonBuffer.clear();
  delete [] msg2;
}



void loop(void){
  if (millis() > api_lasttime + api_mtbs)  {
    digitalWrite(LED_BUILTIN, LOW);
    extractJSON(tcr.searchTwitter(search_str));
    Serial.print("Search: ");
    Serial.println(search_str.c_str());
    Serial.print("MSG: ");
    Serial.println(search_msg.c_str());
    api_lasttime = millis();

    timeClient.update();
    Serial.println(timeClient.getFormattedTime());
    
  }
  delay(2);
  yield();
  digitalWrite(LED_BUILTIN, HIGH);
}
