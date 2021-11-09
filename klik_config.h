
#define WOLD true

/*********************
 * Configuration stuff
 */
const int waitfor = 1000;      // wait for n cycles, used for delays between sensor readings etc.
const int sampleinterval = 10; // sample every 5 minutes
// const char group = 'J';     // switch group
const int maxswitch = 8;       // number of switches
const int switchinterval = 3;  // switch every three minutes
/***************
 * Network stuff
 */
const char* ssid = "********";
const char* password = "********";
const char* sitename = "klik";
/*********************
 * Sensor and LED pins
 **/
const int leds = 3;
const int ledPins[] = {0, 2, 14};
const int transmitPin = 13;
// const int receivePin = 15;

const int temperaturePin = 12; // DHT temperature and humidity sensor
const int lightPin = 0;        // Analog pin for the light resistor
