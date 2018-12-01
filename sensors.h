/**
 * Sensor definitions for klik sensorthings
 */


const int nthings = 1;
Thing things[nthings];

const int nstreams = 5;
Datastream streams[nstreams];

/**
 * Time series arrays
 */
const int nsamples = 15;
float temperatureTS[nsamples];
float humidityTS[nsamples];
float pressureTS[nsamples];
String timeTS[nsamples];
