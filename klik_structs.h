
/**
 * Automatic switch
 */
struct SwitchCommand {
  int no;
  int whenOn;   // When to switch in seconds since midnight
  int whenOff;  // When to switch in seconds since midnight
  boolean on;
};

struct Switch {
  int  no;
  char group;
  String name;
};

/**
 * Sensorthings stuff
 */
struct Thing {
  String name;
  String description;
};

struct UnitofMeasurement {
  String name;
  String symbol;
  String uri;
};

struct Datastream{
  String name;
  String description;
  String observationtype;
  UnitofMeasurement unit;
};

struct Observation {
  float result;
};
