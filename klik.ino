/*****************************************************************
 * House switch and sensor server. Runs on a Wemos D1R2 board
 * with a ESP8266 wifi chip. Connects to the house network, measures
 * the environment and sends commands to the klikaanklikuit switches.
 *
 * Versions
 * 201707 Fully functional but too slow web interface, uses sprint in loop()
 *        reads temperature sensor, pressure sensor and light resistor
 *        and sends out klikaanklikuit commands
 * 201708 completely new setup() and loop(), 
 *        fast web interface css style headers, 
 *        clock function with regular NTP sync.
 *        conversion of time to calendar
 *        cron-like switching times for automatic switching
 * 201804 Corrected some errors in light measurement
 * 201805 Rewrote scheduling, many changes from testing,
 *        different handling of dark/light
 * 201808 Added units to json response, changed Pa to hPa for pressure
 * 201812 Started on Sensorthings
 *
 * Todo:
 * - sensorthings protocol
 * - observations and measurements xml output
 * - log of measurements, send out time series
 * - draw graph of sensor values using SVG
 *
 * Wemos boards use different PIN:
 * Pin  Function        ESP-8266 Pin
 * TX   TXD             TXD
 * RX   RXD             RXD
 * A0   Analog input, max 3.3V input  A0      Light resisitor
 * D0   IO                            GPIO16  Button
 * D1   IO, SCL                       GPIO5   Pressure BMP180 WIRE
 * D2   IO, SDA                       GPIO4   Pressure BMP180 WIRE DATA
 * D3   IO, 10k Pull-up               GPIO0   LED 0
 * D4   IO, 10k Pull-up, BUILTIN_LED  GPIO2   LED 1
 * D5   IO, SCK                       GPIO14  LED 2
 * D6   IO, MISO                      GPIO12  Temperature DHT22
 * D7   IO, MOSI                      GPIO13  Transmit
 * D8   IO, 10k Pull-down, SS         GPIO15  Receive?
 * G    Ground          GND
 * 5V   5V              -
 * 3V3  3.3V            3.3V
 * RST  Reset           RST
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <RemoteSwitch.h>
#include <RemoteReceiver.h>
#include "DHT.h"
#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <WiFiUdp.h>
#include "klik_structs.h"  // Structures
#include "sensors.h";

const char* ssid = "*******";
const char* password = "*******";
const char* sitename = "klik";

/*****************************************************************
 * Sensor and LED pins
 **/
const int leds = 3;
const int ledPins[] = {0, 2, 14};

const int transmitPin = 13;
// const int receivePin = 15;

const int temperaturePin = 12; // DHT temperature and humidity sensor
const int lightPin = 0;        // Analog pin for the light resistor

const int waitfor = 1000;
const int sampleinterval = 5; // sample every 5 minutes
int ncount = 10000000; // Number of cycles between sensor checks
int icount = 0;        // Loop counter for main loop(), get sensor values every ncount cycles
float temperature, humidity, pressure, lux, vout;
int isample;
String webString = "";

// WiFiServer server(80);
ESP8266WebServer server(80);
DHT dht(temperaturePin, DHT22, 24); // the last parameter is some weird number needed because the wemos is too fast for the temperature sensor
KaKuSwitch kaKuSwitch(transmitPin);
Adafruit_BMP085 bmp;

/*****************************************************************
 * To get time from an NTP server
 **/
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;   //Replace with your GMT offset (seconds)
const int   daylightOffset_sec = 3600;  //Replace with your daylight offset (seconds
time_t epoch, epoch0; // Unix time in seconds since 1970
time_t sampleTime, lastCheck;

/*****************************************************************
 * Automatic switches, switch on when it is dark
 */
const float dark = 0.03;       // it's dark below this voltage
// const char group = 'J';        // switch group
const int nswitch = 8;         // number of switches
const int switchinterval = 3;  // switch every three minutes

SwitchCommand sc[nswitch];
Switch light[nswitch];
  
/*****************************************************************
 * Master switch
 **/
const int buttonSwitch = 16;
boolean buttonState, lastButtonState;
boolean klik = true;

/*****************************************************************
 * Setup()
 **/
void setup() {

  /***********************
   * Lights and scheduling
   **/
  light[0] = {1, 'J', "Keuken"};
  light[1] = {2, 'J', "Lotek"};
  light[2] = {3, 'J', "Leeslamp"};   
  light[3] = {4, 'J', "Hal"};
  
  /**
   * Scheduling
   * number, time to switch on, time to switch off
   **/
  sc[0] = {0, timetosec("17:00:00"), timetosec("22:30:00"), false};
  sc[1] = {1, timetosec("14:00:00"), timetosec("22:00:00"), false};
  sc[2] = {2, timetosec("14:05:00"), timetosec("22:30:00"), false};
  sc[3] = {3, timetosec("18:30:00"), timetosec("22:30:00"), false};
  sc[4] = {1, timetosec("06:30:00"), timetosec("08:00:00"), false};
  sc[5] = {0, timetosec("04:45:00"), timetosec("08:00:00"), false};

  /**
   * Sensorthiscngs stuff 
   **/
  things[0] = {"Klik", "Klik meet in huis."};
   
  Serial.begin(19200);
  delay(10);

  // Red LED on during setup()
  led(0, "on");

  // Connect to WiFi network
  Serial.print("Connecting to ");
  Serial.print(ssid);
  Serial.print(" as ");
  Serial.println(sitename);
  WiFi.hostname(sitename);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  // Print the IP address
  Serial.print("Use this URL : ");
  Serial.print("http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");
  
  // Start the sensors
  dht.begin();
  bmp.begin();

  // Init LED pins
  Serial.println("Init LEDs");
  for (int thisPin = 0; thisPin < leds; thisPin++) {
    pinMode(ledPins[thisPin], OUTPUT);
  }

  // Button pin on input
  pinMode(buttonSwitch, INPUT);
  
  server.on("/", []() {
    String message = "<html><head>";
    message += styleHeader();
    message += "</head><body>";
    message += "<p>Switches</p>";
    message += "<form method=\"get\" action=\"/home\"><table>";
    for (int is=0;is<nswitch;is++) {
      if (!light[is].name.equals("")) {
        String name = light[is].name;
        String number = (String) light[is].no;
        message += tableRow(button(name+" aan", "buttonOn",  "switch", number+",on"),
                            button(name+" uit", "buttonOff", "switch", number+",off"));
      }
    }
    message += "</table></form>";
 
    message += "<p>Sensors</p>";
    message += "<table>";
    message += tableHead("parameter", "value");
    message += tableRow("temperature (C)",(String)temperature);
    message += tableRow("humidity (%)",   (String)humidity);
    message += tableRow("pressure (hPa)", (String)pressure);
    message += tableRow("light (V)",      (String)vout);
    message += tableRow("time",           asctime(localtime(&epoch)));
    message += "</table>";
    
    message += "<p>Metingen over een periode van "+(String) (nsamples*sampleinterval)+" minuten.\n";
    message += "Met een meting iedere "+(String) sampleinterval+" minuten.</p>\n";
    message += "<p><center>\n";
    message += graph(nsamples);
    message += "</center>";
    
    message += "</body></html>";
    server.send(200, "text/html", message);
    Serial.println("Request for / handled");
    led(0,"flash");
  });

  server.on("/graph", []() {
    String message = "<html><head>";
    message += styleHeader();
    message += "</head><body>\n";
    message += "<h2>Graph</h2>\n";
    message += "<p>Metingen over een periode van "+(String) (nsamples*sampleinterval)+" minuten.\n";
    message += "Met een meting iedere "+(String) sampleinterval+" minuten.</p>\n";
    message += "<p><center>\n";
    message += graph(nsamples);
    message += "</center></p></body></html>";
    server.send(200, "text/html", message);
    Serial.println("Request for /graph handled");
    led(0,"flash");
  });
  /************************
   * KlikaanKlikuit command
   */
  server.on("/home", []() {
    // String message = "Number of args received:";
    // message += (String)server.args() + "\n";
    String message = "";
    for (int i = 0; i < server.args(); i++) {
      // Split argument in switch number and commmand
      int s1 = server.arg(i).indexOf(",");
      int s2 = server.arg(i).length();
      String sw = server.arg(i).substring(0, s1);
      int is = sw.toInt()-1;
      String cm = server.arg(i).substring(s1 + 1, s2);
      message += "switch number " + sw + " " + cm + "\n";
      boolean cl;
      if (cm.equals("on"))  {
        cl = true;
      }
      if (cm.equals("off")) {
        cl = false;
      }
      if (cm.equals("maybe")) { // maybe only switches on when it is dark
        cl = itsLight();
      }
      if (cm.equals("twilight")) { // keep trying to switch on until it is dark
        // TODO
      }
      
      Serial.print(message);
      // Send out the switch command three times because what i tell you three times is true
      for (int iklik = 0; iklik < 3; iklik++) {
        kaKuSwitch.sendSignal(light[is].group, light[is].no, cl);
        delay(25);
      }
    }
    String page = switchPage(message);
    server.send(200, "text/html", page);
    led(2, "flash");
  });

  /**
   * Various sensor responses
   */
  server.on("/temperature", []() {
    getTemperature();
    String message = "{\"temperature\": " + (String)temperature + ", \"unit_of_measurement\": \"°C\" }";
    server.send(200, "application/json", message);
  });
  server.on("/humidity", []() {
    getTemperature();
    String message = "{\"humidity\": " + (String)humidity + ", \"unit_of_measurement\": \"\%\" }";
    server.send(200, "application/json", message);
  });
  server.on("/pressure", []() {
    getPressure();
    String message = "{\"pressure\": " + (String)(pressure) + ", \"unit_of_measurement\": \"hPa\" }";
    server.send(200, "application/json", message);
  });
  server.on("/light", []() {
    getLight();
    String message = "{\"light\": " + (String)lux + ", \"unit_of_measurement\": \"Volt\" }";
    server.send(200, "application/json", message);
  });
  
  server.on("/sensors", []() {
    String message = "{ \"sample\": [\n";
    // message += "  {\"date\": \"" + printDate(epoch) + "\"},\n";
    // message += "  {\"time\": \"" + printTime(epoch) + "\"},\n";
    message += "  {\"temperature\": { \"value\": " + (String)temperature + ", \"unit_of_measurement\": \"°C\" }},\n";
    message += "  {\"humidity\": { \"value\": " + (String)humidity + ", \"unit_of_measurement\": \"%\" }},\n";
    message += "  {\"pressure\": { \"value\": " + (String)(pressure) + ", \"unit_of_measurement\": \"hPa\" }},\n";
    message += "  {\"voltage over light resistor\": { \"value\": " + (String)vout + ", \"unit_of_measurement\": \"Volt\" }}\n";
    message += "  ]\n";
    message += "}\n";
    server.send(200, "application/json", message);
  });

  /** 
   * Sensorthings stuff
   */
  server.on("/sensorthings/v1.0" , []() {
    String message = "{ \"value\": [";
    message += "  {\"name\": \"Things\", \"url\": \""+server.uri()+"/sensorthings/v1.0/Things\" },";
    message += "  {\"name\": \"Datastreams\", \"url\": \""+server.uri()+"/sensorthings/v1.0/Datastreams\" },";
    // message += "  {\"name\": \"Sensors\", \"url\": "+server.uri()+"/sensorthings/v1.0/Sensors\" },";
    message += "  {\"name\": \"Observations\", \"url\": \""+server.uri()+"/sensorthings/v1.0/Observations\" },";
    // message += "  {\"name\": \"ObservedProperties\", \"url\": \""+server.uri()+"/sensorthings/v1.0/ObservedProperties\" },";
    message += "  {\"name\": \"FeaturesOfInterest\", \"url\": \""+server.uri()+"/sensorthings/v1.0/FeaturesOfInterest\" }";
    message += "] }";
    server.send(200, "application/json", message );
  });
  
  server.on("/sensorthings/v1.0/Things", []() {
    String ID = server.arg("ID");
    /** if (ID == "on") doeiets; **/
    String message = "{ \"value\": [";
    for (int i=0;i<nthings;i++) {
       if (i>0) {message += ", ";}
       message += "{ \"name\": \""+things[i].name+"\", \"description\": \""+things[i].description+"\" }";
    }
    message += "] }";
    server.send(200, "application/json", message );
  });
  
  /**
   * Send out *all* observations in memory
   */
  server.on("/sensorthings/v1.0/Observations", []() {
    String message = "{ \"value\": [\n";
    for (int i=0;i<nsamples;i++) {
       int is = (i + isample) % nsamples;
       if (i>0) {message += ", ";}
       // message += "{ \"@iot.id\": \"t"+(String)(is)+"\", \"result\": \""+(String)temperatureTS[is]+"\", \"phenomenonTime\": \""+timeTS[is]+"\", \"unitOfMeasurement\": \"°C\", \"FeatureOfInterest@iot.navigationLink\": \""+server.uri()+"(t"+(String)(is)+")\"}, ";
       // message += "{ \"@iot.id\": \"h"+(String)(is)+"\", \"result\": \""+(String)humidityTS[is]   +"\", \"phenomenonTime\": \""+timeTS[is]+"\", \"unitOfMeasurement\": \"\%\", \"FeatureOfInterest@iot.navigationLink\": \""+server.uri()+"(h"+(String)(is)+")\"}, ";
       // message += "{ \"@iot.id\": \"p"+(String)(is)+"\", \"result\": \""+(String)pressureTS[is]   +"\", \"phenomenonTime\": \""+timeTS[is]+"\", \"unitOfMeasurement\": \"hPa\", \"FeatureOfInterest@iot.navigationLink\": \""+server.uri()+"(p"+(String)(is)+")\"}";

       message += "{ \"@iot.id\": \"t"+(String)(is)+"\", \"result\": \""+(String)temperatureTS[is]+"\", \"phenomenonTime\": \""+timeTS[is]+"\", \"unitOfMeasurement\": \"°C\"}, \n";
       message += "{ \"@iot.id\": \"h"+(String)(is)+"\", \"result\": \""+(String)humidityTS[is]   +"\", \"phenomenonTime\": \""+timeTS[is]+"\", \"unitOfMeasurement\": \"%\"}, \n";
       message += "{ \"@iot.id\": \"p"+(String)(is)+"\", \"result\": \""+(String)pressureTS[is]   +"\", \"phenomenonTime\": \""+timeTS[is]+"\", \"unitOfMeasurement\": \"hPa\"}";
    }
    message += "] }";
    server.send(200, "application/json", message );
  });

  // Start the server
  server.begin();
  Serial.println("Server started");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  epoch  = getTime();
  
  isample = 0;
  // LEDs off
  led(0, "off");
  led(1, "off");
  led(2, "off");
  
  /**
   * Initial sensor readings
   **/
  getPressure();
  getTemperature();
  getLight();
  Serial.println("Sensors geinitialiseerd");

  Serial.println("Init done");
}

/*****************************************************************
 * The main loop handles the HTTP requests and reads the sensors
 * The sensors are read every ncount cycle
 */
void loop(void) {

  /**
   * Get current time
   **/
  struct tm * timeinfo;
  time (&epoch);
  timeinfo = localtime (&epoch);
  Serial.println(asctime(timeinfo));

  /** 
   * Handle server response
   **/
  server.handleClient();

  /** 
   *  Switch pressed
   */
  buttonState = digitalRead(buttonSwitch);
  if (buttonState == HIGH && lastButtonState == LOW) {
    Serial.println("Hoofdschakelaar");
    for (int iswitch = 0; iswitch < nswitch; iswitch++) {
      for (int iklik = 0; iklik < 3; iklik++) {
        kaKuSwitch.sendSignal(light[iswitch].group, light[iswitch].no, klik);
        delay(20);
      }
    }
    klik = !klik; 
    led(2,"flash");
  }
  lastButtonState = buttonState;
  
  /************************************************
   * Get sensor values every sampleinterval seconds
   **/
  if (difftime(epoch,sampleTime)>60) {
    sampleTime = epoch;
    getTemperature();
    getPressure();
    getLight();
    epoch  = getTime();
    Serial.println(asctime(localtime(&epoch)));
    // store the current values into the array
    temperatureTS[isample] = temperature;
    humidityTS[isample] = humidity;
    pressureTS[isample] = pressure;
    timeTS[isample] = asctime(localtime(&epoch));
    isample++;
    if (isample==nsamples) {isample=0;};
    // LED flashing also gives a 1 second delay, don't remove this line
    led(1, "flash");
  }
    
  /*******************************
   * Handle the scheduled switches
   **/
  if (difftime(epoch,lastCheck)>sampleinterval*60) {
    lastCheck = epoch;
    getLight();
    for (int is=0;is<nswitch;is++) {
      int ilight = sc[is].no;
      // Light should be on
      if ( difftime(epoch,sc[is].whenOn)>0 && difftime(sc[is].whenOff,epoch)<0 ){
        if (!sc[is].on && !itsLight() ) { // but it isn't and it's dark, so send out an "on" command
          for (int iklik = 0; iklik < 3; iklik++) {
            kaKuSwitch.sendSignal(light[ilight].group, light[ilight].no, true);
            delay(250);
          }
          sc[is].on=true;
        }
      } else { // light should be off
        if (sc[is].on) { // but it isn't, so send out the "off" command
          for (int iklik = 0; iklik < 3; iklik++) {
            kaKuSwitch.sendSignal(light[ilight].group, light[ilight].no, false);
            delay(250);
          }
          sc[is].on=false;
        }
      }
    }
  }
  
  // Reset all switches once a day
  // if (now.dsec = 0) {for (int is=0;is<nswitch;is++) {sc[is].done = false;}}
}

/*****************************************************************
 * Just to make the LED switch a little easier
 */
void led(int l, String cm) {
  if (cm.equals("on"))  {
    digitalWrite(ledPins[l], HIGH);
  }
  if (cm.equals("off")) {
    digitalWrite(ledPins[l], LOW);
  }
  if (cm.equals("flash")) {
    digitalWrite(ledPins[l], HIGH);
    delay(waitfor);
    digitalWrite(ledPins[l], LOW);
  }
}

/*****************************************************************
 * getTime() gets a rough time from the number of seconds running
 */
time_t getTime() {
  time_t rawtime;
  struct tm * timeinfo;
  time (&rawtime);
  timeinfo = localtime (&rawtime);
  Serial.println(asctime(timeinfo));
  delay(1000);
  return rawtime;
} 
// From "2:00:00" to 7200   
int timetosec(String d) {
  int s1 = d.indexOf(":");
  int s2 = d.indexOf(":",s1);
  int s3 = d.length();
  int h = d.substring(0,s1).toInt();
  int m = d.substring(s1+1,s2).toInt();
  int s = d.substring(s2+1,s3).toInt();
  int t = h*3600+m*60+s;
  return t;
}

/*****************************************************************
 * Get the air pressure and store it
 */
float getPressure() {
  pressure = bmp.readPressure() / 100.0;
  Serial.print("{pressure:");
  Serial.print(pressure);
  Serial.println("}");
  return pressure;
}

/*****************************************************************
 * Read temperature sensor
 */
float getTemperature() {
  temperature = dht.readTemperature();
  delay(100);
  humidity = dht.readHumidity();
  Serial.print("{temperature:");
  Serial.print(temperature);
  Serial.print(",humidity:");
  Serial.print(humidity);
  Serial.println("}");
  return temperature;
}

/*****************************************************************
 * Get light value from the light resistor
 */
float getLight() {
  float vin = 3.3; //input voltage
  float refresist = 10000.0;
  int adc = analogRead(lightPin);
  
  vout = adc*(vin/1024.0);
  
  float ldresist = refresist * (vin - vout) / vout;
  lux = 1e7 * pow(ldresist, (float) -1.5); // pretty rough estimate
  // if (!itsLight()) {led(2, "flash");} else {led(2, "off");}
  
  Serial.print("{light, ADC:");
  Serial.print(adc);
  Serial.print(", lumen:");
  Serial.print(lux);
  Serial.println("}");

  return vout;
}

/**********************************************************
 * Retuns true if the light resistiance is low (it's light)
 */
boolean itsLight() {
  float vin = 3.3; //input voltage
  int adc = analogRead(lightPin);
  vout = adc*(vin/1024.0);
  if (vout > dark) {return true;}
  return false;
}

/*****************************************************************
 * Print the table header
 * p  parameter
 * v  value
 */
String tableHead(char* p, char* v) {
  String out = "<tr><th>";
  out += p;
  out += "</th><th>";
  out += v;
  out += "</th></tr>";
  return out;
}

/*****************************************************************
 * Print a single table row with a parameter, value pair
 * t  text
 * v  value
 */
String tableRow(String t, String v) {
  String out = "<tr><td>";
  out += t;
  out += "</td><td>";
  out += v;
  out += "</td></tr>";
  return out;
}

/*****************************************************************
 * html button
 * t  text
 * c  class: buttonOn, buttonOff, ButtonMaybe
 * n  name (switch)
 * v  value (1,on)
 */
String button(String t, String c, String n, String v) {
  String out = "<button ";
  out += "class=\"" + c + "\" ";
  out += "type=\"submit\" ";
  out += "name=\"" + n + "\" ";
  out += "value=\"" + v + "\">";
  out += t;
  out += "</button>";
  return out;
}

/**
 * Returns a graph of the measurements in SVG
 * First get minimum and maximum of all time series
 * The measurement arrays are filled and refilled with the 
 * latest value at isample-1, the oldest is thus at isample.
 * So we start plotting from isample to isample-1 mod nsamples.
 */
String graph(int ns) {
  int width=600;
  int height=400;
  float hMin= 100.0; float hMax=    0.0;
  float pMin=2000.0; float pMax=    0.0;
  float tMin=  50.0; float tMax=  -50.0;
  for (int i=0;i<nsamples;i++) {
      tMin = min(temperatureTS[i],tMin);
      tMax = max(temperatureTS[i],tMax);
      hMin = min(humidityTS[i],hMin);
      hMax = max(humidityTS[i],hMax);
      pMin = min(pressureTS[i],pMin);
      pMax = max(pressureTS[i],pMax);
  }
  String out = "<svg height=\""+(String) height+"\" width=\""+(String) width+"\">\n";
  out += "<polyline style=\"fill:none;stroke:green;stroke-width:3\" points=\"";
  for (int i=0;i<nsamples;i++) {
    int is = (i+isample) % nsamples;
    float x=width*i/nsamples;
    float y=height*(1.0-(pressureTS[is]-pMin)/(pMax-pMin));
    out += (String) x+","+(String) y+" ";
  }
  out += "\"/>\n";
  out += "<polyline style=\"fill:none;stroke:blue;stroke-width:3\" points=\"";
  for (int i=0;i<nsamples;i++) {
    int is = (i+isample) % nsamples;
    float x=width*i/nsamples;
    float y=height*(1.0-(humidityTS[is]-hMin)/(hMax-hMin));
    out += (String) x+","+(String) y+" ";
  }
  out += "\"/>\n";
  out += "<polyline style=\"fill:none;stroke:red;stroke-width:3\" points=\"";
  for (int i=0;i<nsamples;i++) {
    int is = (i+isample) % nsamples;
    float x=width*i/nsamples;
    float y=height*(1.0-(temperatureTS[is]-tMin)/(tMax-tMin));
    out += (String) x+","+(String) y+" ";
  }
  out += "\"/>\n";
  out += "</svg>";
  return out;
}

/*****************************************************************
 * Send out the stylesheet header
 */
String styleHeader() {
  String out = "<style>";
  out += " body {background-color: #ffffff; font-family: sans-serif; font-size: 14pt;}";
  out += " table {width: 80%; margin-left:auto; margin-right:auto; font-size: 14pt;}";
  out += " th, td {border-bottom: 1px solid #ddd;}";
  out += " tr:nth-child(odd) {background-color: #f2f2f2}";
  out += " .buttonOn    {background-color: #4CAF50; border-radius: 10%; font-size: 24pt;}";
  out += " .buttonMaybe {background-color: #008CBA; border-radius: 10%; font-size: 24pt;}";
  out += " .buttonOff   {background-color: #f44336; border-radius: 10%; font-size: 24pt;}";
  out += "</style>";
  out += "<meta http-equiv=\"refresh\" content=\"60; url=/\">";
  return out;
}

/*****************************************************************
 * html page response with an instant redirect to the root
 * t  text
 */
String switchPage(String t) {
  String out = "<html><head>";
  out += styleHeader();
  out += "</head><body>";
  out += "<meta http-equiv=\"refresh\" content=\"1; url=/\">";
  out += "</head><body><center>";
  out += t;
  out += "</center></body></html>";
  return out;
}
