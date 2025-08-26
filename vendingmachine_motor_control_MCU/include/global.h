#ifndef GLOBAL_H
#define GLOBAL_H

#include <Arduino.h>
#include <Wire.h>
#include <command_handling.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_INA219.h>

// Define what is compiled
// #define CURRENT_SENSE_ONLY
// #define CURRENT_LOGGING_ON
// #define LOGGER_TX 17

extern Adafruit_INA219 ina219;
extern HardwareSerial Logger;
extern SemaphoreHandle_t androidConfirmation;

// WIFI details
#define AP_NAME "ENTER_WIFI_NAME"
#define AP_PASSWORD "ENTER_WIFI_PASSWORD"

// MQTT details
#define mqtt_server "ENTER_MQTT_SERVER_IP"
#define mqtt_port 1884 // Default setting, edit based on actual MQTT port
#define mqtt_incoming_topic "topic/hostToClient"
#define mqtt_outgoing_topic "topic/clientToHost"

struct commandStruct {
    int len;
    char charArray[COMMAND_MAX_LEN];
};

#endif