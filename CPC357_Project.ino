#include <VOneMqttClient.h>
#include <vonesetting.h>
#include "DHT.h"
#define DHTTYPE DHT11

const char* dht11_id = "33200482-ab89-480d-9c3a-e6dd93edbffa"; // DHT11 SSID
const char* button_id = "beccc0b2-4edb-476d-8e4d-f005050dce63"; // Button SSID

VOneMqttClient vOneClient;

const int dht11_pin = 21; // For DHT11 pin
const int led_green = 19; // For green LED to simulate on/off
const int relay_pin = 32; // For Octocoupler pin that controls the fan (ventilation)
const int button_pin = 33; // For push button pin
const int idle = 3000; // Time taken before going to sleep mode

unsigned long current = millis(); // Place holders for time taken
unsigned long previous = 0;

bool active = false; // Flags to keep track of shaft motor and LED
bool flag = true;
float humidity = 0; // Placeholders values for humidity and temperature
float temperature = 0;

DHT dht(dht11_pin, DHTTYPE);

void setup_wifi() {
  delay(10);
  
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  } 
  
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}


// Check if button is clicked which puts the system to sleep and later wakes up.
void IRAM_ATTR button_click() {
  flag = !flag;
  active = true;
  previous = millis();

  switch(flag) {
    case true:
      digitalWrite(led_green, HIGH);
      digitalWrite(relay_pin, true);
      break;
    
    case false:
      digitalWrite(led_green, LOW);
      digitalWrite(relay_pin, false);

      Serial.print("\n==========================\n\n");
      Serial.println("Pressure plate stepped. System going to sleep...");
      Serial.print("\n==========================\n\n");
      
      esp_deep_sleep_start();
      break;

    default:
      Serial.println("An error occured");
  }
}

// Display wakeup reason system wakes up
void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:     Serial.println("Wakeup caused by pressure plate."); break;
    default:                        Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.print("\n==========================\n\n");
  print_wakeup_reason();
  Serial.println("Welcome.");

  // Set the sensors and acutators pin mode
  pinMode(button_pin, INPUT_PULLUP);
  pinMode(led_green, OUTPUT);
  pinMode(relay_pin, OUTPUT);

  // Set the default behaviour of the devices
  digitalWrite(led_green, LOW);
  digitalWrite(relay_pin, false);

  setup_wifi(); // Connect to the WiFi network
  vOneClient.setup();

  dht.begin();
  attachInterrupt(digitalPinToInterrupt(button_pin), button_click, HIGH); // Function call when button is clicked
  esp_sleep_enable_ext0_wakeup((gpio_num_t)button_pin, LOW); // Function call to wake up the system when button is clicked

  esp_log_level_set("*", ESP_LOG_NONE);

  if (esp_reset_reason() == ESP_RST_DEEPSLEEP) {
    Serial.println("\nSystem turning on...");
    Serial.print("\n==========================\n\n");
  }
}

void loop() {
  delay(5000);
  current = millis();

  // Check if client is unable to connect to cloud platform and restarts it
  if (!vOneClient.connected()) {
    vOneClient.reconnect();
    vOneClient.publishDeviceStatusEvent(dht11_id, true);
    vOneClient.publishDeviceStatusEvent(button_id, true);
  }
  vOneClient.loop();

  if(active && ((current - previous) > idle)) {
    active = false;
  }

  int button_val = !digitalRead(button_pin); // Reads button value
    
  humidity = dht.readHumidity(); // Reads humidity value
  temperature = dht.readTemperature(); // Reads temeprature value

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor.");
    return;
  }

  Serial.print("Temperature (C): ");
  Serial.print(temperature);
  Serial.print(" Humidity (%): ");
  Serial.print(humidity);
  Serial.print("\n");

  // Check if the humidity or temperature is above/below the threshold to turn off the LED and shaft motor
  if (humidity > 90 || temperature > 25) {
    digitalWrite(led_green, HIGH);
    digitalWrite(relay_pin, true);
  }
  else {
    digitalWrite(led_green, LOW);
    digitalWrite(relay_pin, false);
  }

  if (button_val == HIGH){ button_val = 1; }
  else { button_val = 0; }

  JSONVar payloadObject;    
  payloadObject["Humidity"] = humidity;
  payloadObject["Temperature"] = temperature;
  vOneClient.publishTelemetryData(dht11_id, payloadObject); // Publish Humidity and Temperature data to cloud platform
  vOneClient.publishTelemetryData(button_id, "Button1", button_val); // Publish button reading to cloud platform
}