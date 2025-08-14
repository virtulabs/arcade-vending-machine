#include <global.h>
#include <command_handling.h>
#include <motor_control.h>
#include <diagnostics.h>

WiFiClient espClient;
PubSubClient client(espClient);
bool initialSetupDone = false;

void setup_wifi() {
  delay(10);
  Logger.print("Connecting to ");
  Logger.println(AP_NAME);

  WiFi.begin(AP_NAME, AP_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Logger.print(".");
  }
  Logger.println("WiFi connected");
}

void setup_mqtt() {
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);
  reconnect();
  initialSetupDone = true;
}

void reconnect() {
  Logger.println("[Logger] Attempting MQTT connection...");
  while (!client.connect("ESP32Client")) {
    Logger.print("[Logger] MQTT connection failed, rc=");
    Logger.print(client.state());
    Logger.println(" try again in 5 seconds");
    delay(5000);
  }
  Logger.printf("[Logger] MQTT connected via port %d on server %s\n", mqtt_port, mqtt_server);
  client.subscribe(mqtt_incoming_topic);
}

bool isMQTTConnected() {
  if (client.connected()) {return true;}
  else {return false;}
}

void sendMQTTResponse(const char *input) {
  client.publish(mqtt_outgoing_topic, input);
}

/*
Simple switch function helper to convert row/col char input to relevant int row/col idx in motorStateMatrix
Returns 255 if invalid row/col char input
*/
uint8_t charToMatrixIdx(char input) {
  switch (input) {
    // Rows
    case 'A':
    case 'a':
      return 0;
    case 'B':
    case 'b':
      return 1;
    case 'C':
    case 'c':
      return 2;
    case 'D':
    case 'd':
      return 3;
    case 'E':
    case 'e':
        return 4;
    case 'F':
    case 'f':
        return 5;

    // Columns
    case '1':
      return 0;
    case '2':
      return 1;
    case '3':
      return 2;
    case '4':
      return 3;
    case '5':
      return 4;
    case '6':
      return 5;
    case '7':
      return 6;
    case '8':
      return 7;

    default:
      return 255;
  }
}

/*
Simple switch function helper to handle multiple valid row inputs (eg. 'a' or 'A' are valid) and reject invalid inputs (eg. '\0' or '~' are invalid)
Returns '\0' if invalid row char input
*/
char rowValidator(char row) {
    switch (row) {
    case 'A':
    case 'a':
      return 'A';
    case 'B':
    case 'b':
      return 'B';
    case 'C':
    case 'c':
      return 'C';
    case 'D':
    case 'd':
      return 'D';
    case 'E':
    case 'e':
        return 'E';
    case 'F':
    case 'f':
        return 'F';

    default:
      return '\0';
  }
}

/*
Dequeues commands (char array) from commandQueue and executes based on [action;cell] structure
Dispense actions are executed only for indicated cell, stop and test actions are executed for all cells regardless of indication
Prints received action over serial + success/error
*/
void commandHandler(void * params) {
  commandStruct command;

  while (true) {
    // Check if there are commands in the queue
    if (uxQueueMessagesWaiting(commandQueue) > 0) {
      if (xQueueReceive(commandQueue, &command, 0) == pdPASS) {
        Logger.print("[Logger] [commandHandler] Command received: ");
        Logger.println(command.charArray);
        char action[14]; // eg. "disp", "stop", "test"
        char cell[3] = {'\0', '\0', '\0'}; // eg. "a4", "b2"
        char row = '\0', col = '\0';
        
        char delimiter = ';';
        char * delimiterPos = strchr(command.charArray, delimiter);
        if (delimiterPos != NULL) { // Found delimiter, valid command received
          size_t actionLen = delimiterPos - command.charArray;
          strncpy(action, command.charArray, actionLen);
          action[actionLen] = '\0';
          Logger.print("[Logger] [commandHandler] action: ");
          Logger.println(action);

          // Parse row and col
          if (*(delimiterPos + 1)) {row = rowValidator(*(delimiterPos + 1));}
          if (*(delimiterPos + 2)) {col = *(delimiterPos + 2);}
          Logger.printf("[Logger] [commandHandler] row: %c, col: %c\n", row, col);

          if (strcmp("disp", action) == 0) {
            if (!row | charToMatrixIdx(col) == 255) {
              char errorResponse[64];
              snprintf(errorResponse, sizeof(errorResponse), "INVALID CELL %c%c", row, col);
              Serial.println(errorResponse);
              if (isMQTTConnected()) {
                sendMQTTResponse(errorResponse);
              }
              delay(5);
              continue;
            }
            uint8_t result = runMotorOneRev(row, col);
            setMotorState(row, col, result);
            
            char response[64];
            switch (result) {
              case 0:
                snprintf(response, sizeof(response), "%s DONE", action);
                break;
              case 1:
                snprintf(response, sizeof(response), "%s ERROR 1: CURRENT OUTLIER", action);
                break;
              case 2:
                snprintf(response, sizeof(response), "%s ERROR 2: HOME TIMEOUT", action);
                break;
              case 3:
                snprintf(response, sizeof(response), "%s ERROR 3: MOTOR FLAGGED", action);
                break;
            }
            
            // Send response to both Serial and MQTT
            Serial.println(response);
            if (isMQTTConnected()) {
              sendMQTTResponse(response);
            }
          } else if (strcmp("stop", action) == 0) {
            powerOffAll();
            const char* response = "stop DONE";
            Serial.println(response);
            if (isMQTTConnected()) {
              sendMQTTResponse(response);
            }

          } else if (strcmp("test", action) == 0) {
            if (!row && !col) { // Test whole system
              testSystemMotorState();
            } else if (row && !col) {
              // Test row motor state
              testSystemMotorState(row);
            } else {
              // Test row motor state
              testSystemMotorState(row, col);
            }
            const char* response = "test DONE";
            Serial.println(response);
            if (isMQTTConnected()) {
              sendMQTTResponse(response);
            }
            
          } else if (strcmp("rst", action) == 0) { // Resets specific motor flag
            if (!row && !col) { // Reset whole motorStateMatrix
              for (uint8_t i = 0; i < sizeof(motorStateMatrix) / sizeof(motorStateMatrix[0]); i++) {
                for (uint8_t j = 0; j < sizeof(motorStateMatrix[i]) / sizeof(motorStateMatrix[i][0]); j++) {
                    motorStateMatrix[i][j] = 0;
                }
              }
            } else if (row && !col) { // Reset one row of motorStateMatrix
                uint8_t rowIdx = charToMatrixIdx(row);
                for (uint8_t i = 0; i < sizeof(motorStateMatrix[rowIdx]) / sizeof(motorStateMatrix[rowIdx][0]); i++) {
                    motorStateMatrix[rowIdx][i] = 0;
                }
            } else if (row && col) {
                setMotorState(row, col, 0);
            } else {
              char errorResponse[64];
              snprintf(errorResponse, sizeof(errorResponse), "INVALID CELL %c%c", row, col);
              Serial.println(errorResponse);
              if (isMQTTConnected()) {
                sendMQTTResponse(errorResponse);
              }
              delay(5);
              continue;
            }
            const char* response = "rst DONE";
            Serial.println(response);
            if (isMQTTConnected()) {
              sendMQTTResponse(response);
            }
            
          } else if (strcmp("send", action) == 0) { // Send motorStateMatrix to android tablet
            sendMotorStateMatrix();
            const char* response = "send motorStateMatrix DONE";
            Serial.println(response);
            if (isMQTTConnected()) {
              sendMQTTResponse(response);
            }
          }
        }
      }
    }
    delay(5);
    // Monitor stack usage every 1000 loops
    static int counter = 0;
    if (++counter >= 10000) {
      counter = 0;
      UBaseType_t watermark = uxTaskGetStackHighWaterMark(NULL);
      Logger.printf("[Logger] [commandHandler] Stack high water mark: %u\n", watermark);
    }
  }
}

// TODO for android device: swap to this command structure
/*
sample commands
disp;a1\n
stop;\n
test;c3\n
test;\n
Nothing after delimiter means 'all' for stop and test. disp will never activate all motors
*/

/*
Buffers full commands (terminator '\n') into commandQueue as commandStruct (len and charArray)
Drops command if commandQueue is full or received command length is invalid
Prints RECEIVE SUCCESS/FAIL over serial
*/
void serialHandler(void * params) {
  commandStruct serialBuffer;

  while (true) {
    if (Serial.available() >= 7) { // wait for minimum command size before reading
      int len = Serial.readBytesUntil('\n', serialBuffer.charArray, COMMAND_MAX_LEN - 1);
      if (len <= 0 || len >= COMMAND_MAX_LEN) { // Handle invalid command lengths
        Logger.println("[Logger] [serialHandler] Command length invalid, discarding input");
        while (Serial.available() > 0) {
          Serial.read(); // Flush buffer
          delay(1);
        }
        Serial.println("RECEIVE FAIL");
        continue;
      } else {
        serialBuffer.charArray[len] = '\0';
        serialBuffer.len = len;

        if (strncmp("rowreceived", serialBuffer.charArray, 9) == 0) { // Confirmation received from Android
          xSemaphoreGive(androidConfirmation);
          continue; // Don't enqueue confirmation (not an action)
        }
        if (xQueueSend(commandQueue, &serialBuffer, portMAX_DELAY) == errQUEUE_FULL) {
          Logger.println("[Logger] [serialHandler] Command queue full, dropping command");
          Serial.println("RECEIVE FAIL"); // TODO for android device: resend command if "RECEIVE FAIL"
        } else {
          Logger.println("[Logger] [serialHandler] Command enqueued");
          Serial.println("RECEIVE SUCCESS"); // TODO for android device: only move on to next command if "RECEIVE SUCCESS"
        }
      }
    }
    // Monitor stack usage every 10000 loops
    static int counter = 0;
    if (++counter >= 10000) {
      counter = 0;
      UBaseType_t watermark = uxTaskGetStackHighWaterMark(NULL);
      Logger.printf("[Logger] [serialHandler] Stack high water mark: %u\n", watermark);
    }
    client.loop();
    delay(5);
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Logger.printf("[Logger] Received on MQTT: %s\n", payload);
  commandStruct mqttBuffer;

  if (strcmp(topic, mqtt_incoming_topic) != 0) {return;}

  // Reject incorrect payload lengths
  if (length <= 0 || length >= COMMAND_MAX_LEN) { // Handle invalid command lengths
    Logger.println("[Logger] [mqttCallback] Command length invalid, discarding input");
    return;
  }

  // Copy payload to charArray
  memcpy(mqttBuffer.charArray, payload, length);
  mqttBuffer.charArray[length] = '\0';
  mqttBuffer.len = length;

  if (strncmp("rowreceived", mqttBuffer.charArray, 9) == 0) { // Confirmation received from Android
    xSemaphoreGive(androidConfirmation);
    return; // Don't enqueue confirmation (not an action)
  }

  if (xQueueSend(commandQueue, &mqttBuffer, portMAX_DELAY) == errQUEUE_FULL) {
    Logger.println("[Logger] [serialHandler] Command queue full, dropping command");
    Serial.println("RECEIVE FAIL"); // TODO for android device: resend command if "RECEIVE FAIL"
  } else {
    Logger.println("[Logger] [serialHandler] Command enqueued");
    Serial.println("RECEIVE SUCCESS"); // TODO for android device: only move on to next command if "RECEIVE SUCCESS"
  }
}

void checkMQTT(void * params) {
  while (1) {
    // Once initial MQTT setup is done, if MQTT disconnects this will get triggered
    if (!isMQTTConnected() && initialSetupDone) {
        reconnect();
    }
    delay(100);
  }
}