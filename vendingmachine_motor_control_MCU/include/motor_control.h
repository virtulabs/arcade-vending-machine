#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include "PCAL9535A.h"
#include <Wire.h>
#define SDA_2 14
#define SCL_2 13
#define I2C_FREQ 10000
extern PCAL9535A::PCAL9535A<TwoWire> relayGPIO;

// Key and value lists (emulating a dict) storing relay row/col to GPIO pin mappings
extern const char row_keys[6]; // Row index
extern uint8_t row_values[6]; // ESP32 GPIO row pin
extern const char col_keys[8]; // Col index
extern uint8_t col_values[8]; // ESP32 GPIO col pin

extern bool areAnyRelaysOn;
extern const float motorResistor;

// Function declarations
void relayPinSetup();

bool relayControl(char row, char col, uint8_t mode);
uint8_t getPin(char index);
void powerOffAll();
void checkRelayPower();

void sendMotorHome(char row, char col); // Turn motor to home position (uses similar logic/process as runMotorOneRev)
uint8_t runMotorOneRev(char row, char col); // Start motor then poll current sensor, take rolling average during revolution period, wait for current spike following revolution period. If current spike not received before timeout, return FALSE

#endif