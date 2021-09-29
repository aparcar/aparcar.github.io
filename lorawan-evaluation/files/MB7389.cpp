#include <softSerial.h>

uint32_t get_sonic_distance(softSerial softwareSerial, uint32_t count) {
  boolean foundValue;
  boolean foundStart;
  String inString;
  uint32_t measurements = 0;
  uint32_t total = 0;
  uint32_t start;

  softwareSerial.begin(9600);
  while (!softwareSerial.available()) {
  }

  // Run loop until enough measurements were read
  while (measurements < count) {
    // Since measurements may fail resulting in a retry after 1/6 seconds, store
    // the start time so one a measurement is successfully read, start the next
    // measurement exactly a second later.
    start = millis();

    // Reset variables used to find measurement
    foundStart = false;
    foundValue = false;
    inString = "";

    // Flush software serial input
    softwareSerial.flush();

    // Loop until a single measurement is found
    while (!foundValue) {
      if (softwareSerial.available()) {
        char inChar = softwareSerial.read();
        if (foundStart == false) {
          // New measurements begin with the character `R`
          if (inChar == 'R') {
            foundStart = true;
          } else {
            // If not, skip the letter and repeat
            softwareSerial.read();
          }
        } else {
          // Measurements end with character `\r`. If any other value is found
          // add it to our measurement.
          if (inChar != '\r') {
            inString += inChar;
          } else {
            // If found character is `\r` and therefore end of a measurement,
            // check if the length is actually four character. If not, repeat
            // the process.
            if (inString.length() != 4) {
              Serial.println("incomplete measurement");
              foundStart = false;
              inString = "";
            } else {
              // Found available measurement, increase counter and begin again
              foundValue = true;
              total += inString.toInt();
              measurements += 1;
              // Wait 1 second minus the time it took to read the last
              // measurement
              delay(1000 - (millis() - start));
            }
          }
        }
      } else {
        // Give the software serial more time to read characters
        delay(50);
      }
    }
  }
  // Return average of measurements and convert it to cm (sensor returns mm)
  return (total / (count * 10)); // to cm
}
