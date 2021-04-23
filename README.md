# Timebox2

Open the box on startup/boot and close it when a configured time is reached.

It also spawns a tiny webserver and displays a few buttons to play around with the servo.

The configuration is fetched from a remote server and looks like this:
```json
{
"events": [
    {"start_date":"*", "start_time":"23:01", "end_date":"*","end_time":"22:30"},
    {"start_date":"23.4.2021", "start_time":"22:29", "end_date":"23.04.2021","end_time":"22:30"},
    {"start_date":"23.4.2021", "start_time":"22:29", "end_date":"23.04.2021","end_time":"22:30"}
  ]
}
```
A * means "every day", usable for recurring meetings like Scrum dailies.

Check out the defines at the top of the code to adjust to your needs.

## Requirements

Hardware:

- Wemos D1 (or clone)
- Servo (I used a cheap micro SG90 clone)
- 3D printed box (STLs here)

Libraries:

- ESP8266WebServer, ESP8266WiFi and ESP8266HTTPClient via https://arduino.esp8266.com/stable/package_esp8266com_index.json
- ArduinoJson from Benoit Blanchon


## Requirements

- Brown servo cable to Wemos ground
- Red servo cable to Wemos 3.3V
- Yellow servo cable to D5 (or change port in the code accordingly)
