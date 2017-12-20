#include <AWS_IOT.h>
#include <ArduinoJson.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#else
#include <WiFi.h>          //https://github.com/esp8266/Arduino
#endif

//needed for library
#if defined(ESP8266)
#include <ESP8266WebServer.h>
#else
#include <WebServer.h>
#endif
#include <DNSServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

AWS_IOT hornbill;
#define TRIGGER_PIN 0
#define LED_PIN 2

char HOST_ADDRESS[] = "a2oxjrmrtmst02.iot.us-west-2.amazonaws.com";
char CLIENT_ID[] = "TruongESP32";
char TOPIC_NAME[] = "$aws/things/TruongESP32/shadow/update";

int tick = 0, msgCount = 0, msgReceived = 0, reConfig = 0;
char payload[512];
char rcvdPayload[512];

void mySubCallBackHandler (char *topicName, int payloadLen, char *payLoad) {
  strncpy(rcvdPayload, payLoad, payloadLen);
  rcvdPayload[payloadLen] = 0;
  msgReceived = 1;
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(TRIGGER_PIN, INPUT);
  Serial.begin(115200);

  WiFiManager wifiManager;
  wifiManager.resetSettings();

  if ((reConfig == 1) || (!wifiManager.autoConnect("AutoConnectAP"))) {
    Serial.println("ok vao roi");
    if (!wifiManager.startConfigPortal("OnDemandAP")) {
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

}

void loop() {
  if (digitalRead(TRIGGER_PIN) == LOW) {
    reConfig = 1;
    setup();
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
    sprintf(payload, "{\"state\": { \"desired\": { \"welcome\": %d }}}", msgCount++);
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
