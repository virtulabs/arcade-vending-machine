#include <global.h>
#include <motor_control.h>
#include <diagnostics.h>

bool areAnyRelaysOn = false;
const float motorResistor = 500.0;

// Key and value lists (emulating a dict) storing relay row/col to I2C relay GPIO pin mappings
const char row_keys[6] = {'A', 'B', 'C', 'D', 'E', 'F'}; // Row index
uint8_t row_values[6] = {0, 1, 2, 3, 4, 5}; // Relay GPIO row pin
const char col_keys[8] = {'1', '2', '3', '4', '5', '6', '7', '8'}; // Col index
uint8_t col_values[8] = {8, 9, 10, 11, 12, 13, 14, 15}; // Relay GPIO col pin

// Helper function to set all relay GPIOs to output mode
void relayPinSetup() {
  for (uint8_t i = 0; i < 16; i++) {
    relayGPIO.pinMode(i, OUTPUT);
    delay(10);
  }
}

/*
Controls relay to selected motor based on mode input
mode == 0: power off (row & col LOW), 
mode == 1: power on (row & col HIGH), 
mode == 2: test motor (col HIGH, row LOW)
Outputs bool, TRUE = action successful, FALSE = action not successful due to existing motor error flag
*/
bool relayControl(char row, char col, uint8_t mode) {
  // Relay in NO configuration (LOW - current flowing, HIGH - current not flowing)
  
  // If in power on mode, check motor state. If motor has been flagged, stop operation
  if (mode == 1) {
    uint8_t motor_state = getMotorState(row, col);
    if (motor_state != 0) {
      Serial.printf("Error;Motor %c%c;Flag %u\n", row, col, motor_state);
      return false;
    }
  }

  uint8_t row_pin = getPin(row);
  uint8_t col_pin = getPin(col);

  if (mode == 0) { // power off
    relayGPIO.digitalWrite(row_pin, LOW);
    relayGPIO.digitalWrite(col_pin, LOW);
  } else if (mode == 1) { // power on
    checkRelayPower();
    relayGPIO.digitalWrite(row_pin, HIGH);
    relayGPIO.digitalWrite(col_pin, HIGH);
    areAnyRelaysOn = true;
  } else { // test mode
    relayGPIO.digitalWrite(row_pin, HIGH);
  }
  return true;
}

// Loops through row & col indexes to return respective GPIO pin
uint8_t getPin(char index) {
  for (uint8_t i = 0; i < sizeof(row_keys)/sizeof(row_keys[0]); i++) {
    if (row_keys[i] == index) {
      return row_values[i];
    }
  }
  for (uint8_t i = 0; i < sizeof(col_keys)/sizeof(col_keys[0]); i++) {
    if (col_keys[i] == index) {
      return col_values[i];
    }
  }
  return 255;
}

// Opens all relays, cutting power to all motors
void powerOffAll() {
  for (uint8_t i = 0; i < 16; i++) {
    relayGPIO.digitalWrite(i, LOW);
    delay(10);
  }
  areAnyRelaysOn = false;
}

/*
Helper function to check that all relays are closed/on (before further action)
If relays are closed/on, open/off all (using powerOffAll)
*/ 
void checkRelayPower() {
  if (areAnyRelaysOn) {
    powerOffAll();
  }
}

void sendMotorHome(char row, char col) {
  float i_total = 0.0; // Rolling sum of current readings in mA
  float i_ave = 0.0; // Rolling average of current in mA
  float home_delta = 24.0 / motorResistor * 1000.0 - 10.0; // Delta to detect home state after revolution period (24V / 500ohm * 1000mA - allowance)
  uint16_t poll_count = 0;
  uint16_t home_count = 0;
  uint8_t poll_interval =  5;
  uint16_t delay_period = 100;
  const unsigned long timeout = 3000;

  relayControl(row, col, 1);

  unsigned long start_time = millis();

  delay(delay_period);

  while (home_count < 5) {
    float i_curr = ina219.getCurrent_mA();

    if (millis() - start_time > timeout) {
      Logger.printf("[Logger] Motor %c%c home timeout error!\n", row, col);
      break;
    }

    if (i_curr - i_ave > home_delta && poll_count > 2) {
      #ifdef CURRENT_LOGGING_ON
      Logger.printf("[Logger] Home, i_curr = %f\t", i_curr);
      #endif
      home_count += 1;
    } else {
      home_count = 0;
      poll_count += 1;
      i_total += i_curr;
      i_ave = i_total / poll_count;
      #ifdef CURRENT_LOGGING_ON
      Logger.printf("[Logger] Log, i_ave = %f\t", i_ave);
      #endif
    }
    
    delay(poll_interval);
  }

  relayControl(row, col, 0);
}

/*
Supplies power to selected motor, conducts current sensing to cut power when home condition is detected
Returns uint8_t: 0 = home reached successfully, 1 = current outlier error, 2 = home timeout error, 3 = motor previously flagged and not cleared
*/
uint8_t runMotorOneRev(char row, char col) {

  float i_total = 0.0; // Rolling sum of current readings in mA
  float i_ave = 0.0; // Rolling average of current in mA
  float outlier_delta = 50.0; // Delta for outlier rejection during revolution period (mA)
  float home_delta = 24.0 / motorResistor * 1000.0 - 10.0; // Delta to detect home state after revolution period (24V / 500ohm * 1000mA - allowance)

  uint16_t poll_count = 0;
  uint8_t outlier_count = 0;
  uint8_t max_allowable_outliers = 20;
  uint8_t home_count = 0;
  uint8_t poll_interval = 5; // Interval between polls
  uint16_t delay_period = 250; // Motor starts turning < t < Cam leaves home position
  uint16_t revolution_period = 1500; // Cam leaves home position < t < Cam almost returning to home position
  uint16_t timeout = 4000; // If home return not detected before timeout, return false

  // Logger.printf("[Logger] Motor %c%c: 'I'm working on it boss'\n",row,col);
  if (!relayControl(row, col, 1)) {return 3;}
  unsigned long start_time = millis();

  // Delay until out of home position
  delay(delay_period);

  while (home_count < 5) {
    float i_curr = ina219.getCurrent_mA();

    if (millis() - start_time > timeout) {
      relayControl(row, col, 0);
      Logger.printf("[Logger] Motor %c%c home timeout error!\n", row, col);
      return 2;
    }

    if (i_curr - i_ave > home_delta && poll_count > 250) {
      #ifdef CURRENT_LOGGING_ON
      Logger.printf("[Logger] Home, i_curr = %f\t", i_curr);
      #endif
      home_count += 1;
    } else {
      home_count = 0;
      poll_count += 1;
      i_total += i_curr;
      i_ave = i_total / poll_count;
      #ifdef CURRENT_LOGGING_ON
      Logger.printf("[Logger] Log, i_ave = %f\t", i_ave);
      #endif
    }
    delay(poll_interval);
  }

  relayControl(row, col, 0);
  return 0; // Successfully reached home
}