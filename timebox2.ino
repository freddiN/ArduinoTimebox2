#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <time.h>   
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

#define PIN_SERVO D5

#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define WIFI_HOSTNAME "timebox"

#define MY_NTP_SERVER "de.pool.ntp.org"
#define MY_TZ "CET-1CEST,M3.5.0,M10.5.0/3"  //Germany, Berlin

#define SERVO_ANGLE_OPEN 8      // going too close to 0 causes jitter
#define SERVO_ANGLE_CLOSE 108   // going too close to 180 causes jitter

#define EVENTS_URL "http://192.168.178.24/events.json"  //see events.json on github for an example

#define INTERVAL_CHECK_TIME 10000         //10 seconds in ms
#define INTERVAL_DOWNLOAD_JSON 36000000   //10 hours in ms

unsigned long time_now_a = 0, time_now_b = 0;
boolean isInMeeting = false;
String strJson = "";
time_t now;
tm tm;  

ESP8266WebServer server(80);

void setup() {
  Serial.begin(115200);
  while(!Serial) { 
  }

  pinMode(PIN_SERVO, OUTPUT); 

  connectToWifi();

  server.on(F("/"), handleRoot);
  server.begin();

  configTime(MY_TZ, MY_NTP_SERVER);

  updateJson();   //get the JSON initially

  time(&now);
  localtime_r(&now, &tm);
}

void loop() {
  server.handleClient();

  if((unsigned long)(millis() - time_now_a) > INTERVAL_CHECK_TIME){
    time_now_a = millis();
    checkTimes();
  }

  if((unsigned long)(millis() - time_now_b) > INTERVAL_DOWNLOAD_JSON){
    time_now_b = millis();
    updateJson();
  }
}

void checkTimes() {  
  time(&now);
  localtime_r(&now, &tm); //update the global times, good enough to do this every 10 seconds
  
  if (strJson.length() > 10) {
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, strJson);

    const int arraySize = doc["events"].size();
    for (int i = 0; i< arraySize; i++){
      const char* start_date = doc["events"][i]["start_date"];
      const char* start_time = doc["events"][i]["start_time"];
      const char* end_date = doc["events"][i]["end_date"];
      const char* end_time = doc["events"][i]["end_time"];

      if (!isInMeeting && timeMatches(start_time) && dateMatches(start_date)) {
        performServo(SERVO_ANGLE_OPEN);
        isInMeeting = true;
      } else if (isInMeeting && timeMatches(end_time) && dateMatches(end_date)) {
        performServo(SERVO_ANGLE_CLOSE);
        isInMeeting = false;
      }
    }
  }
}

/** 
 * strTime format 22:29
 */
bool timeMatches(const String& strTime) {
  const String strHour   = getValue(strTime, ':', 0);
  const String strMinute = getValue(strTime, ':', 1);

  return (strHour.toInt() == tm.tm_hour && strMinute.toInt() == tm.tm_min);
}

/**
 * strDate format 23.04.2021 or *, but never on weekends!
 */
bool dateMatches(const String& strDate) {
  if(tm.tm_wday == 0 || tm.tm_wday == 6) {
    return false;
  }
  
  if (strDate == F("*")) {
    return true;
  }

  const String strDay   = getValue(strDate, '.', 0);
  const String strMonth = getValue(strDate, '.', 1);
  const String strYear  = getValue(strDate, '.', 2);
  
  return (strDay.toInt() == tm.tm_hour && strMonth.toInt() == (tm.tm_mon + 1) && strYear.toInt() == (tm.tm_year + 1900));
}

void performServo(const int& pos) {
  for (int i=0; i<40; i++) {
    setServo(pos);
  }
}

void setServo(const int& pos) {
  const int puls = map(pos,0,180,400,2400);
  digitalWrite(PIN_SERVO, HIGH);
  delayMicroseconds(puls);
  digitalWrite(PIN_SERVO, LOW);
  delay(19);
}

void handleRoot() {
  String strAngle = F("-1");

  if (server.arg("angle") != ""){
    strAngle = server.arg("angle");
  }

  String HTML;
  HTML.reserve(2048);
  HTML = F("<!DOCTYPE html>\n");
  HTML += F("<html>\n");
  HTML += F("<head><title>Timebox</title></head>\n");
  HTML += F("<body>\n");
  HTML += F("<h2>Timebox</h2>\n<p>\nip: ");

  HTML += WiFi.localIP().toString();
  HTML += F("<br>\nWIFI signal: ");
  
  HTML += WiFi.RSSI();
  HTML += F("<br>\nangle: ");
  HTML += strAngle;
  HTML += F("<br>\ntimestamp: ");

  char buffer[18];
  sprintf(buffer, "%02d.%02d.%d %02d:%02d", tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900, tm.tm_hour, tm.tm_min); // 25.04.2021 01:45

  HTML += buffer;
  HTML += F("<br>\nin meeting: ");
  HTML += isInMeeting;
  HTML += F("<br>\nJSON: ");
  HTML += strJson;
  HTML += F("<br>\n<br>\n</p>\n");

  HTML += generateForm(F("0"), F("0"));
  HTML += generateForm(F("45"), F("45"));
  HTML += generateForm(F("90"), F("90"));
  HTML += generateForm(F("135"), F("135"));
  HTML += generateForm(F("180"), F("180"));
  HTML += generateForm(F("1000"), F("Up"));
  HTML += generateForm(F("2000"), F("Down"));
  HTML += generateForm(F("3000"), F("Update JSON"));

  HTML += F("</body>\n</html>\n");

  server.send(200, F("text/html"), HTML);

  if (strAngle.toInt() >= 0 && strAngle.toInt() <= 180) {
    performServo(strAngle.toInt());
  } else if (strAngle.toInt() == 1000) {
    performServo(SERVO_ANGLE_OPEN);
  } else if (strAngle.toInt() == 2000) {
    performServo(SERVO_ANGLE_CLOSE);
  } else if (strAngle.toInt() == 3000) {
    updateJson();
  }
}

String generateForm(const String& strAngle, const String& strText) {
  String HTML;
  HTML.reserve(160);
  HTML = F("<form action=\"/\" method=\"GET\">\n");
  HTML += F("  <input type=\"hidden\" name=\"angle\" value=\""); 
  HTML += strAngle;
  HTML += F("\"/>\n");
  HTML += F("  <input type=\"submit\" value=\"");
  HTML += strText;
  HTML += F("\">\n");
  HTML += F("</form>\n");
  return HTML;
}

void connectToWifi() {
  Serial.print(F("Connecting..."));
  WiFi.persistent(false);
  WiFi.hostname(WIFI_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);  
  //try to connect 20 times 0.5 seconds apart -> for 10 seconds
  int i = 20;
  while(WiFi.status() != WL_CONNECTED && i >=0) {
    delay(500);
    Serial.print(i);
    Serial.print(F(", "));
    i--;
  }
  Serial.println(F(" "));

  if(WiFi.status() == WL_CONNECTED){
    Serial.println(F("Connected!")); 
    Serial.print(F("ip address: ")); 
    Serial.println(WiFi.localIP());
    const long rssi = WiFi.RSSI();
    Serial.print(F("RSSI:"));
    Serial.println(rssi);
  } else {
    Serial.println(F("Connection failed - check your credentials or connection"));
  }
}

void updateJson() {
  HTTPClient http;
  http.begin(EVENTS_URL);
   
  if (http.GET() == 200) {
    strJson = http.getString();
  }
   
  http.end();
}

/**
 * String xval = getValue(myString, ':', 0);
 */
String getValue(const String& data, const char& separator, const int& index){
  int found = 0;
  int strIndex[] = { 0, -1 };
  const int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}
