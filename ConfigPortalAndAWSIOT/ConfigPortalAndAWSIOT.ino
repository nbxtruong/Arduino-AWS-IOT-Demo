#include <AWS_IOT.h>
#include <ArduinoJson.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

//needed for library
#if defined(ESP8266)
#include <ESP8266WebServer.h>
#else
#include <WebServer.h>
#endif
#include <DNSServer.h>
#include <WiFiManager.h>

//Library for DHT
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

//Connect to DHT by using PIN 14
#define DHTPIN 14

// Uncomment the type of sensor in use:
//#define DHTTYPE DHT11     // DHT 11
#define DHTTYPE DHT22     // DHT 22 (AM2302)
//#define DHTTYPE DHT21     // DHT 21 (AM2301)

DHT_Unified dht(DHTPIN, DHTTYPE);

AWS_IOT hornbill;
#define TRIGGER_PIN 0
#define LED_PIN 2
WiFiManager wifiManager;

char HOST_ADDRESS[] = "a2oxjrmrtmst02.iot.us-west-2.amazonaws.com";
char CLIENT_ID[] = "TruongESP32";
char TOPIC_NAME[] = "$aws/things/TruongESP32/shadow/update";

int tick = 0, msgCount = 0, msgReceived = 0, reConfig = 0;
float temperature = 0.0, humidity = 0.0;
char payload[512];
char rcvdPayload[512];
uint32_t delayMS;

// List of callback function
void mySubCallBackHandler (char *topicName, int payloadLen, char *payLoad) {
  strncpy(rcvdPayload, payLoad, payloadLen);
  rcvdPayload[payloadLen] = 0;
  msgReceived = 1;
}

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
}
// End list of callback function

void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(TRIGGER_PIN, INPUT);
  Serial.begin(115200);

  wifiManager.setAPCallback(configModeCallback);

  if (!wifiManager.autoConnect()) {
    if (!wifiManager.startConfigPortal()) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      ESP.restart();
      delay(5000);
    }
  }

  Serial.println("Connected to wifi");

  if (hornbill.connect(HOST_ADDRESS, CLIENT_ID) == 0) {
    Serial.println("Connected to AWS");
    delay(1000);

    if (0 == hornbill.subscribe(TOPIC_NAME, mySubCallBackHandler)) {
      Serial.println("Subscribe Successfull");
    }
    else {
      Serial.println("Subscribe Failed, Check the Thing Name and Certificates");
      while (1);
    }
  }
  else {
    Serial.println("AWS connection failed, Check the HOST Address");
    while (1);
  }

  delay(2000);

  // Initialize DHT device.
  dht.begin();
  Serial.println("DHTxx Unified Sensor Example");
  // Print temperature sensor details.
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  Serial.println("------------------------------------");
  Serial.println("Temperature");
  Serial.print  ("Sensor:       "); Serial.println(sensor.name);
  Serial.print  ("Driver Ver:   "); Serial.println(sensor.version);
  Serial.print  ("Unique ID:    "); Serial.println(sensor.sensor_id);
  Serial.print  ("Max Value:    "); Serial.print(sensor.max_value); Serial.println(" *C");
  Serial.print  ("Min Value:    "); Serial.print(sensor.min_value); Serial.println(" *C");
  Serial.print  ("Resolution:   "); Serial.print(sensor.resolution); Serial.println(" *C");
  Serial.println("------------------------------------");
  // Print humidity sensor details.
  dht.humidity().getSensor(&sensor);
  Serial.println("------------------------------------");
  Serial.println("Humidity");
  Serial.print  ("Sensor:       "); Serial.println(sensor.name);
  Serial.print  ("Driver Ver:   "); Serial.println(sensor.version);
  Serial.print  ("Unique ID:    "); Serial.println(sensor.sensor_id);
  Serial.print  ("Max Value:    "); Serial.print(sensor.max_value); Serial.println("%");
  Serial.print  ("Min Value:    "); Serial.print(sensor.min_value); Serial.println("%");
  Serial.print  ("Resolution:   "); Serial.print(sensor.resolution); Serial.println("%");
  Serial.println("------------------------------------");
  // Set delay between sensor readings based on sensor details.
  delayMS = sensor.min_delay / 1000;
}

void loop() {
  // Delay between measurements.
  delay(delayMS);
  // Get temperature event and print its value.
  sensors_event_t event;
  dht.temperature().getEvent(&event);
  if (isnan(event.temperature)) {
    Serial.println("Error reading temperature!");
  }
  else {
    temperature = event.temperature;
  }
  // Get humidity event and print its value.
  dht.humidity().getEvent(&event);
  if (isnan(event.relative_humidity)) {
    Serial.println("Error reading humidity!");
  }
  else {
    humidity = event.relative_humidity;
  }

  wifiManager.setAPCallback(configModeCallback);

  if (digitalRead(TRIGGER_PIN) == LOW) {
    wifiManager.resetSettings();
    wifiManager.startConfigPortal();
  }

  if (msgReceived == 1) {
    msgReceived = 0;
    Serial.print("Received Message:");
    Serial.println(rcvdPayload);

    // Parsing json
    StaticJsonBuffer<300> JSONBuffer;   //Memory pool
    JsonObject& parsed = JSONBuffer.parseObject(rcvdPayload);

    // Check for errors in parsing
    if (!parsed.success()) {
      Serial.println("Parsing failed");
      delay(5000);
      return;
    }

    // Get value of "welcome"
    int value = parsed["state"]["desired"]["welcome"];
    if (value % 2 == 0) {
      digitalWrite(2, HIGH);
      delay(1000);
    } else {
      digitalWrite(2, LOW);
      delay(1000);
    }
  }

  if (tick >= 5) { // publish to topic every 5seconds
    tick = 0;
    sprintf(payload, "{\"state\": { \"desired\": { \"welcome\": %d, \"temperature\": %f, \"humidity\": %f }}}", msgCount++, temperature, humidity );
    if (hornbill.publish(TOPIC_NAME, payload) == 0) {
      Serial.print("Publish Message:");
      Serial.println(payload);
    }
    else {
      Serial.println("Publish failed");
    }
  }
  vTaskDelay(1000 / portTICK_RATE_MS);
  tick++;
}
