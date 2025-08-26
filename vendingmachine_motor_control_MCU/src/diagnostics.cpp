#include <global.h>
#include <diagnostics.h>
#include <command_handling.h>
#include <motor_control.h>

uint8_t motorStateMatrix[6][8] = {
    {0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0}
};

/* 
Sets motor state in appropriate cell of motorStateMatrix 
(0: functional, 1: outlier error, 2: timeout error, 3: shortcircuit error) 
*/
void setMotorState(char row, char col, uint8_t state) {
  uint8_t rowIdx = charToMatrixIdx(row);
  uint8_t colIdx = charToMatrixIdx(col);
  if (rowIdx == 255 | colIdx == 255) {
    Logger.println("[Logger] [setMotorState] ERROR: invalid row/col input!");
    return;
  }
  motorStateMatrix[rowIdx][colIdx] = state;
}

/*
Gets motor state from appropriate cell of motorStateMatrix 
(0: functional, 1: outlier error, 2: timeout error, 3: shortcircuit error)
*/
uint8_t getMotorState(char row, char col) {
  uint8_t rowIdx = charToMatrixIdx(row);
  uint8_t colIdx = charToMatrixIdx(col);
  if (rowIdx == 255 | colIdx == 255) {
    Logger.println("[Logger] [getMotorState] ERROR: invalid row/col input!");
    return 255;
  }
  return motorStateMatrix[rowIdx][colIdx];
}

/*
Tests motor state by closing power relay (col) & opening ground relay (row)
Sets motor state in motorStateMatrix (0: functional, 3: shortcircuit error)
*/
void testSingleMotorState(char row, char col, float baseline) {
  float max_idle_current = 1.0;
  uint8_t counter = 0;

  relayControl(row, col, 2);
  for (uint8_t i = 0; i < 20; i++) {
    
    float i_curr = ina219.getCurrent_mA(); // Get current reading

    if (i_curr - baseline > max_idle_current) {counter += 1;}
    if (counter >= 15) {
      relayControl(row, col, 0); // Cut power to motor
      setMotorState(row, col, 3); // Flag motor with shortcircuit error
      Serial.printf("Error;Motor %c%c;Flag 3\n", row, col);
      Logger.printf("[Logger] i_curr = %f, i_baseline = %f\n", i_curr, baseline);
      return;
    }
    delay(5);
  }
  relayControl(row, col, 0);
  setMotorState(row, col, 0);
  Logger.printf("[Logger] Motor %c%c functional\n", row, col);
}

// Establishes baseline then tests motor (single/row/all) state using testSingleMotorState
void testSystemMotorState(char row, char col) {
  // Record baseline current reading
  float i_total = 0;
  checkRelayPower();
  for (uint8_t i = 0; i < 50; i++) {
    i_total += ina219.getCurrent_mA();
    delay(20);
  }
  float i_baseline = i_total / 50;

  if (!row && !col) {
    // Test every motor state in system
    for (uint8_t row_idx = 0; row_idx < sizeof(row_keys)/sizeof(row_keys[0]); row_idx++) {
      for (uint8_t col_idx = 0; col_idx < sizeof(col_keys)/sizeof(col_keys[0]); col_idx++) {
        testSingleMotorState(row_keys[row_idx], col_keys[col_idx], i_baseline);
        delay(5);
      }
    }
  } else if (!col) {
    // Test every motor state in row
    for (uint8_t col_idx = 0; col_idx < sizeof(col_keys)/sizeof(col_keys[0]); col_idx++) {
        testSingleMotorState(row_keys[row], col_keys[col_idx], i_baseline);
        delay(5);
      }
  } else {
    // Test single motor in cell
    testSingleMotorState(row, col, i_baseline);
  }
  Logger.println("[Logger] Test Complete");
}

void sendMotorStateMatrix() {
  uint8_t retry_count = 0;
  Logger.println("[Logger] Sending motorStateMatrix to Android...");
  Serial.println("MOTORSTATEMATRIX;");
  for (uint8_t row_idx = 0; row_idx < sizeof(motorStateMatrix)/sizeof(motorStateMatrix[0]); row_idx++) {
    if (retry_count >= 5) {
      Logger.println("[Logger] [sendMotorStateMatrix] No confirmation received, cancelling send operation.");
      break;
    }
    Serial.printf("ROW%d;", row_idx);
    for (uint8_t col_idx = 0; col_idx < sizeof(motorStateMatrix[row_idx])/sizeof(motorStateMatrix[row_idx][0]); col_idx++) {
      Serial.printf("%d,", motorStateMatrix[row_idx][col_idx]);
    }
    Serial.println();
    if (xSemaphoreTake(androidConfirmation, pdMS_TO_TICKS(2000)) == pdTRUE) {
      Logger.println("[Logger] Android confirmation received");
      retry_count = 0;
    } else {
      Logger.println("[Logger] Android confirmation TIMEOUT. Repeating send");
      row_idx -= 1;
      retry_count += 1;
    }
    delay(5);
  }
}