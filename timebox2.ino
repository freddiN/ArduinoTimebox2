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

unsigned long time_now_a = 0;
unsigned long time_now_b = 0;
boolean isInMeeting = false;
String strJson = "";

ESP8266WebServer server(80);

void setup() {
  Serial.begin(115200);
  while(!Serial) { 
  }

  pinMode(PIN_SERVO, OUTPUT); 

  connectToWifi();

  server.on("/", handleRoot);
  server.begin();

  configTime(MY_TZ, MY_NTP_SERVER);

  updateJson();   //get the JSON initially
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
  if (strJson.length() > 10) {
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, strJson);

    time_t now;
    tm tm;  
    time(&now);
    localtime_r(&now, &tm);

    const int arraySize = doc["events"].size();
    for (int i = 0; i< arraySize; i++){
      const char* start_date = doc["events"][i]["start_date"];
      const char* start_time = doc["events"][i]["start_time"];
      const char* end_date = doc["events"][i]["end_date"];
      const char* end_time = doc["events"][i]["end_time"];

      if (!isInMeeting && timeMatches(tm, start_time) && dateMatches(tm, start_date)) {
        performServo(SERVO_ANGLE_OPEN);
        isInMeeting = true;
      } else if (isInMeeting && timeMatches(tm, end_time) && dateMatches(tm, end_date)) {
        performServo(SERVO_ANGLE_CLOSE);
        isInMeeting = false;
      }
    }
  }
}

/** 
 * strTime format 22:29
 */
bool timeMatches(const tm tm, const String strTime) {
  const String strHour   = getValue(strTime, ':', 0);
  const String strMinute = getValue(strTime, ':', 1);

  return (strHour.toInt() == tm.tm_hour && strMinute.toInt() == tm.tm_min);
}

/**
 * strDate format 23.04.2021 or *, but never on weekends!
 */
bool dateMatches(const tm tm, const String strDate) {
  if(tm.tm_wday == 0 || tm.tm_wday == 6) {
    return false;
  }
  
  if (strDate == "*") {
    return true;
  }

  const String strDay   = getValue(strDate, '.', 0);
  const String strMonth = getValue(strDate, '.', 1);
  const String strYear  = getValue(strDate, '.', 2);
  
  return (strDay.toInt() == tm.tm_hour && strMonth.toInt() == (tm.tm_mon + 1) && strYear.toInt() == (tm.tm_year + 1900));
}

void performServo(const int pos) {
  for (int i=0; i<40; i++) {
    setServo(pos);
  }
}

void setServo(const int pos) {
  const int puls = map(pos,0,180,400,2400);
  digitalWrite(PIN_SERVO, HIGH);
  delayMicroseconds(puls);
  digitalWrite(PIN_SERVO, LOW);
  delay(19);
}

void handleRoot() {
  String strAngle = "-1";

  if (server.arg("angle") != ""){
    strAngle = server.arg("angle");
  }

  String HTML = "<!DOCTYPE html>\n";
  HTML += "<html>\n";
  HTML += "<head><title>Timebox</title></head>\n";
  HTML += "<body>\n";
  HTML += "<h2>Timebox</h2>\n";

  HTML += "<p>";
  HTML += "ip: ";
  HTML += WiFi.localIP().toString();
  HTML += "<br>\n";
  
  HTML += "WIFI signal: ";
  HTML += WiFi.RSSI();
  HTML += "<br>\n";

  HTML += "angle: ";
  HTML += strAngle;
  HTML += "<br>\n";
  HTML += "timestamp: ";
  HTML += getCurrentTime();
  HTML += "<br>\n";
  HTML += "inMeeting: ";
  HTML += isInMeeting;
  HTML += "<br>\n";
  HTML += "JSON: ";
  HTML += strJson;
  HTML += "<br>\n";

  HTML += "<br>\n";

  HTML += "</p>\n";

  HTML += generateForm("0", "0");
  HTML += generateForm("45", "45");
  HTML += generateForm("90", "90");
  HTML += generateForm("135", "135");
  HTML += generateForm("180", "180");
  HTML += generateForm("1000", "Up");
  HTML += generateForm("2000", "Down");
  HTML += generateForm("3000", "Update JSON");

  HTML += "</body>\n";
  HTML += "</html>\n";

  server.send(200, "text/html", HTML);

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

String generateForm(const String strAngle, const String strText) {
  String HTML = "<form action=\"/\" method=\"GET\">\n";
  HTML += "  <input type=\"hidden\" name=\"angle\" value=\"" + strAngle + "\"/>\n";
  HTML += "  <input type=\"submit\" value=\"" + strText +"\">\n";
  HTML += "</form>\n";
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
    long rssi = WiFi.RSSI();
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

String getCurrentTime() {
  time_t now;
  tm tm;  
  time(&now);                       // read the current time
  localtime_r(&now, &tm);           // update the structure tm with the current time
  
  char buffer [50];
  
  String TXT = "";

  if (tm.tm_mday < 10) {
    TXT.concat("0");
  }
  itoa(tm.tm_mday, buffer, 10);
  TXT.concat(buffer);
  TXT.concat(".");

  if (tm.tm_mon + 1 < 10) {
    TXT.concat("0");
  }
  itoa(tm.tm_mon + 1, buffer, 10);
  TXT.concat(buffer);
  TXT.concat(".");

  itoa(tm.tm_year + 1900, buffer, 10);
  TXT.concat(buffer);
  TXT.concat(" ");

  if (tm.tm_hour < 10) {
    TXT.concat("0");
  }
  itoa(tm.tm_hour, buffer, 10);
  TXT.concat(buffer);
  TXT.concat(":");

  if (tm.tm_min < 10) {
    TXT.concat("0");
  }
  itoa(tm.tm_min, buffer, 10);
  TXT.concat(buffer);
  TXT.concat(":");

  if (tm.tm_sec < 10) {
    TXT.concat("0");
  }
  itoa(tm.tm_sec, buffer, 10);
  TXT.concat(buffer);

  return TXT;
}

/**
 * String xval = getValue(myString, ':', 0);
 */
String getValue(const String data, const char separator, const int index){
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}
