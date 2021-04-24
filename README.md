# Timebox2

Open the box at the beginning of an appointment  and close it upon the end.

It spawns a tiny webserver and displays a few buttons to play around:
![image](https://user-images.githubusercontent.com/14030572/115965508-e2aa8400-a529-11eb-8280-4dfb74f9c76d.png)


The configuration is fetched from a remote server and looks like this:
```json
{
"events": [
    {"start_date":"*", "start_time":"10:30", "end_date":"*","end_time":"11:00"},
    {"start_date":"23.4.2021", "start_time":"22:29", "end_date":"23.04.2021","end_time":"22:30"},
    {"start_date":"23.4.2021", "start_time":"22:29", "end_date":"23.04.2021","end_time":"22:30"}
  ]
}
```
A * means "every day", usable for recurring meetings like Scrum dailies.

Check out the defines at the top of the code to adjust to your needs.

IMPORTANT: Disconnect the servo's ground (=brown wire) before flashing or the flash will fail.

## Requirements

Hardware:

- Wemos D1 (or clone)
- Servo (I used a cheap micro SG90 clone)
- 3D printed box (STLs here: https://www.thingiverse.com/thing:4838367)

Libraries:

- ESP8266WebServer, ESP8266WiFi and ESP8266HTTPClient via https://arduino.esp8266.com/stable/package_esp8266com_index.json
- ArduinoJson from Benoit Blanchon
- no Servo lib needed

## Wiring

- Brown servo cable to Wemos ground
- Red servo cable to Wemos 3.3V
- Yellow servo cable to D5 (or change port in the code accordingly)
