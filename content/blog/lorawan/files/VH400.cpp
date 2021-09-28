#include "Arduino.h"

float read_VH400(uint8_t VH400_PIN) {

  float vh400_voltage = analogReadmV(VH400_PIN) / 1000.0;
  float VWC;

  // Calculate VWC based on voltages provided by vendor:
  // https://vegetronix.com/Products/VH400/VH400-Piecewise-Curve.phtml
  if (vh400_voltage <= 1.1) {
    VWC = 10 * vh400_voltage - 1;
  } else if (vh400_voltage <= 1.3) {
    VWC = 25 * vh400_voltage - 17.5;
  } else if (vh400_voltage <= 1.82) {
    VWC = 48.08 * vh400_voltage - 47.5;
  } else if (vh400_voltage <= 2.2) {
    VWC = 26.32 * vh400_voltage - 7.89;
  } else if (vh400_voltage <= 3.0) {
    VWC = 62.5 * vh400_voltage - 87.5;
  } else {
    VWC = 0.0;
  }
  return (VWC);
}
