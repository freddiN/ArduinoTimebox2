#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <time.h>   
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

#define PIN_SERVO D5

#define wifiSsid "YOUR_WIFI_SSID"
#define wifiPassword "YOUR_WIFI_PASSWORD"
#define wifiHostname "timebox"

#define MY_NTP_SERVER "de.pool.ntp.org"
#define MY_TZ "CET-1CEST,M3.5.0,M10.5.0/3"  //Germany, Berlin

#define ANGLE_OPEN 8
#define ANGLE_CLOSE 108

/* Format:
{
"events": [
  {"start_date":"23.4.2021", "start_time":"22:29", "end_date":"23.04.2021","end_time":"22:30"},
  {"start_date":"23.4.2021", "start_time":"22:29", "end_date":"23.04.2021","end_time":"22:30"},
  {"start_date":"*", "start_time":"22:29", "end_date":"*","end_time":"22:30"}
]
}*/
#define EVENTS_URL "http://192.168.178.24/events.json"

#define INTERVAL 10000 //in ms
unsigned long time_now = 0;

ESP8266WebServer server(80);

boolean isInMeeting = false;

void setup() {
  Serial.begin(115200);
  while(!Serial) { 
  }

  pinMode(PIN_SERVO, OUTPUT); 

  connectToWifi();

  server.on("/", handleRoot);
  server.begin();

  configTime(MY_TZ, MY_NTP_SERVER);

  Serial.println("Setup done");
}

void loop() {
  server.handleClient();

  if((unsigned long)(millis() - time_now) > INTERVAL){
    time_now = millis();
    checkData();
  }
}

void checkData() {
  Serial.println("checkData");

  String strGetData = getData();
  if (strGetData.length() > 10) {
    Serial.println("i start_date start_time end_date end_time");
    
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, strGetData);

    time_t now;
    tm tm;  
    time(&now);
    localtime_r(&now, &tm);

    int arraySize = doc["events"].size();
    for (int i = 0; i< arraySize; i++){
      const char* start_date = doc["events"][i]["start_date"];
      const char* start_time = doc["events"][i]["start_time"];
      const char* end_date = doc["events"][i]["end_date"];
      const char* end_time = doc["events"][i]["end_time"];
      Serial.print(i);
      Serial.print(" ");
      Serial.print(start_date);
      Serial.print("   ");
      Serial.print(start_time);
      Serial.print("     ");
      Serial.print(end_date);
      Serial.print("   ");
      Serial.print(end_time);
      Serial.println(" ");

      if (!isInMeeting && timeMatches(tm, start_time) && dateMatches(tm, start_date)) {
        servoUp();
        isInMeeting = true;
      } else if (isInMeeting && timeMatches(tm, end_time) && dateMatches(tm, end_date)) {
        servoDown();
        isInMeeting = false;
      }
    }
  }
}

// 22:29
bool timeMatches(tm tm, String strTime) {
  String strHour   = getValue(strTime, ':', 0);
  String strMinute = getValue(strTime, ':', 1);

  if (strHour.toInt() == tm.tm_hour && strMinute.toInt() == tm.tm_min) {
    return true;
  } else {
    return false;
  }
}

// * or 23.04.2021
bool dateMatches(tm tm, String strDate) {
  if (strDate == "*") {
    return true;
  }

  String strDay   = getValue(strDate, '.', 0);
  String strMonth = getValue(strDate, '.', 1);
  String strYear  = getValue(strDate, '.', 2);
  
  if (strDay.toInt() == tm.tm_hour && strMonth.toInt() == tm.tm_mon + 1 && strYear.toInt() == tm.tm_year + 1900) {
    return true;
  } else {
    return false;
  }
}

void servoUp() {
  Serial.println("servoUp");
  for (int i=0; i<50; i++) {
    setServo(ANGLE_OPEN);
  }
}

void servoDown() {
  Serial.println("servoDown");
  for (int i=0; i<50; i++) {
    setServo(ANGLE_CLOSE);
  }
}

void setServo(int pos) {
  int puls = map(pos,0,180,400,2400);
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
  HTML += "<head>\n";
  HTML += "<title>Timebox</title>\n";
  HTML += "</head>\n";
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
  HTML += isInMeeting                ;
  HTML += "<br>\n";

  HTML += "<br>\n";

  HTML += "</p>\n";

  HTML += generateForm("0");
  HTML += generateForm("45");
  HTML += generateForm("90");
  HTML += generateForm("135");
  HTML += generateForm("180");

  HTML += "</body>\n";
  HTML += "</html>\n";

  server.send(200, "text/html", HTML);

  if (strAngle.toInt() >= 0 && strAngle.toInt() <= 180) {
    for (int i=0; i<40;i++) {
    setServo(strAngle.toInt());
    }
  }

  if (strAngle.toInt() == 1000) {
    servoUp();
  }

  if (strAngle.toInt() == 2000) {
    servoDown();
  }
}

String generateForm(String strAngle) {
  String HTML = "";
  HTML += "<form action=\"/\" method=\"GET\">\n";
  HTML += "  <input type=\"hidden\" name=\"angle\" value=\"";
  HTML += strAngle;
  HTML += "\"/>\n";
  HTML += "  <input type=\"submit\" value=\"";
  HTML += strAngle;
  HTML +="\">\n";
  HTML += "</form>\n";
  return HTML;
}

void connectToWifi() {
  Serial.print("Connecting...");
  WiFi.persistent(false);
  WiFi.hostname(wifiHostname);
  WiFi.begin(wifiSsid, wifiPassword);  
  //try to connect 20 times 0.5 seconds apart -> for 10 seconds
  int i = 20;
  while(WiFi.status() != WL_CONNECTED && i >=0) {
    delay(500);
    Serial.print(i);
    Serial.print(", ");
    i--;
  }
  Serial.println(" ");

  if(WiFi.status() == WL_CONNECTED){
    Serial.print("Connected!"); 
    Serial.println(" ");
    Serial.print("ip address: "); 
    Serial.println(WiFi.localIP());
    long rssi = WiFi.RSSI();
    Serial.print("RSSI:");
    Serial.println(rssi);
  } else {
    Serial.println("Connection failed - check your credentials or connection");
  }
}

String getData() {
  String strReturn = "";
  
  HTTPClient http;
  http.begin(EVENTS_URL);
   
  if (http.GET() == 200) {
    strReturn = http.getString();
  }
   
  http.end();
  return strReturn;
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

String getValue(String data, char separator, int index)
{
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
