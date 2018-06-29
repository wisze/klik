/**
 * Time and date stuff
 **/
struct DateTime {
  int sec;
  int dsec; // seconds since midnight
  int min;
  int hour;
  int dow;
  int day;
  int month;
  int year;
  int config;
  int doy;    // not BCD!
};

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
  int no;
  String name;
};

