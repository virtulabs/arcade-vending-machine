#include <global.h>
#include <motor_control.h>

// Global variable definitions
Adafruit_INA219 ina219;
PCAL9535A::PCAL9535A<TwoWire> relayGPIO(Wire1);

HardwareSerial Logger(0);

TaskHandle_t commandHandlerTaskHandle = NULL;
TaskHandle_t serialHandlerTaskHandle = NULL;
TaskHandle_t checkMQTTStatusHandle = NULL;
QueueHandle_t commandQueue = NULL;

SemaphoreHandle_t androidConfirmation;

void setup(void) 
{
  Serial.begin(115200);

  while (!Serial) {
      delay(1);
  }

  #ifdef LOGGER_TX
  Logger.begin(115200, SERIAL_8N1, -1, LOGGER_TX);
  #else
  Logger.begin(115200);
  #endif
  
  delay(500);

  // Check that WiFi credentials have been set in global.h
  if (strcmp(AP_NAME, "ENTER_WIFI_NAME") == 0 || strcmp(AP_PASSWORD, "ENTER_WIFI_PASSWORD") == 0) {
    while (1) {
      Logger.println("ERROR: Please set your WiFi credentials in global.h, then re-compile and upload the code.");
      delay(2000);
    };
  }

  // Check that MQTT server details have been set in global.h
  if (strcmp(mqtt_server, "ENTER_MQTT_SERVER_IP") == 0) {
    while (1) {
      Logger.println("ERROR: Please set your MQTT server details in global.h, then re-compile and upload the code.");
      delay(2000);
    };
  }

  Logger.println("[Logger] Setting Pin2 as output and driving LOW");
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);

  Logger.println("[Logger] MotorControl Logger beginning setup...");
  Serial.println("MotorControl Serial beginning setup...");
  
  // Initialize I2C bus for relay board
  Wire1.begin(SDA_2, SCL_2, I2C_FREQ);
  relayGPIO.begin();

  // Set up all relay GPIOs and power of (open) all relays
  relayPinSetup();
  powerOffAll();

  // Initialize the INA219.
  while (!ina219.begin()) {
    Logger.println("[Logger] Failed to find INA219 chip, retrying...");
    delay(1000);
  }
  ina219.setCalibration_32V_1A();

  // Create commandQueue and confirm creation before proceeding
  while (1) {
    commandQueue = xQueueCreate(COMMAND_QUEUE_LEN, sizeof(commandStruct));
    if (commandQueue == NULL) {
      Logger.println("[Logger] commandQueue creation failed. Retrying...");
    } else {
      Logger.println("[Logger] commandQueue created.");
      break;
    }
  }
  
  // Create tasks and confirm creation before proceeding
  Logger.println("[Logger] Creating FreeRTOS tasks...");
  
  while (1) {
    if 
    (
      xTaskCreate(commandHandler, "commandHandlerTask", 3072, NULL, 1, &commandHandlerTaskHandle) == pdPASS &&
      xTaskCreate(serialHandler, "serialHandlerTask", 2048, NULL, 1, &serialHandlerTaskHandle) == pdPASS &&
      xTaskCreate(checkMQTT, "checkMQTTTask", 2048, NULL, 1, &checkMQTTStatusHandle) == pdPASS
    ) 
    {
      Logger.println("[Logger] [Main] All tasks created successfully.");
      break;
    } else {
      Logger.println("[Logger] [Main] Task creation failed. Retrying...");
      if (commandHandlerTaskHandle != NULL) {vTaskDelete(commandHandlerTaskHandle);}
      if (serialHandlerTaskHandle != NULL) {vTaskDelete(serialHandlerTaskHandle);}
      if (checkMQTTStatusHandle != NULL) {vTaskDelete(checkMQTTStatusHandle);}
    }
    delay(5);
  }

  // Create global Android confirmation semaphore
  androidConfirmation = xSemaphoreCreateBinary();

  delay(100);
  Logger.println("[Logger] MotorControl Ready");
  Serial.println("MotorControl Ready");

  Logger.println("[Logger] Setting up WiFi...");
  setup_wifi();
  setup_mqtt();
}

void loop(void) {}