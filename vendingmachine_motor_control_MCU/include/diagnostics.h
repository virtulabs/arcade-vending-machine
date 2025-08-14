#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

extern uint8_t motorStateMatrix[6][8];

// Function declarations
void setMotorState(char row, char col, uint8_t state);
uint8_t getMotorState(char row, char col);

void testSingleMotorState(char row, char col, float baseline);
void testSystemMotorState(char row = '\0', char col = '\0');
void sendMotorStateMatrix();

#endif