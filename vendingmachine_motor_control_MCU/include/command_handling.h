#ifndef COMMAND_HANDLING_H
#define COMMAND_HANDLING_H

extern QueueHandle_t commandQueue;
#define COMMAND_QUEUE_LEN 4
#define COMMAND_MAX_LEN 64

extern TaskHandle_t commandHandlerTaskHandle;
extern TaskHandle_t serialHandlerTaskHandle;
extern TaskHandle_t checkMQTTStatusHandle;

extern bool initialSetupDone;

// Function declarations
void commandHandler(void * params); // Dequeue complete command from commandQueue, parse command, execute command, send confirmation over Serial
void serialHandler(void * params); // Look for delimiters, wait for queue slot to free up, buffer complete commands into queue when free
uint8_t charToMatrixIdx(char input);
char rowValidator(char row);

// MQTT & WIFI functions
void setup_wifi();
void setup_mqtt();
void reconnect();
bool isMQTTConnected();
void sendMQTTResponse(const char *input);
void mqttCallback(char* topic, byte* payload, unsigned int length);
void checkMQTT(void * params);

#endif