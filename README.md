# klik

Klik is an application to switch domestic appliances (lights mostly) on and off using klikaanklikuit socket devices. These get their commands over 433MHz.

The application uses a dht temperature and humidity sensor, a bmp pressure and tempeature sensor and a light resistor to measure room lighting. The light resistor is a 5506 type light resistor which pretty much has no resistance when there is sunlight in the room and only yields measurable values under artificial light or around dawn and dusk.

The wemos version has a website (http://klik/ on a local network) with a number of buttons to switch lights on and off.

Timed on and off switch commands are hardcoded. At given times the switch on and off commands are sent automatically. The timed light on commands are sent only when the light resistor indicates it is dark in the room. Lights are switched off automatically whe the light resistor resistance is low (the room is light).

Sensor measurement values can be requested on a number of urls, a request returns json.
- http://klik/temperature returns temperature in Celsius
- http://klik/humidity    returns humidity in %
- http://klik/pressure    returns pressure in hectoPascal
- http://klik/light       returns a rough apprroximation of lux values, returns inf when in daylight
- http://klik/sensors     all of the sensors measurements
