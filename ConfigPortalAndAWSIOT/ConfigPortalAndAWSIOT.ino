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

//Library for OTA
#include <Update.h>

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
WiFiClient wClient;

char HOST_ADDRESS[] = "a2oxjrmrtmst02.iot.us-east-1.amazonaws.com";
char CLIENT_ID[] = "TruongESP32";
char TOPIC_NAME[] = "$aws/things/TruongESP32/shadow/update";

int tick = 0, msgCount = 0, msgReceived = 0, reConfig = 0;
float temperature = 0.0, humidity = 0.0;
char payload[512];
char rcvdPayload[512];
uint32_t delayMS;

// Variables to validate
// response from S3
int contentLength = 0;
bool isValidContentType = false;

// S3 Bucket Config
String host = "esp32dat.s3-ap-southeast-1.amazonaws.com"; // Host => bucket-name.s3.region.amazonaws.com
int port = 80; // Non https. For HTTPS 443. As of today, HTTPS doesn't work.
String bin = "/ConfigPortalAndAWSIOT.ino.doitESP32devkitV1.bin"; // bin file name with a slash in front.

// List of callback function
void mySubCallBackHandler (char *topicName, int payloadLen, char *payLoad) {
  strncpy(rcvdPayload, payLoad, payloadLen);
  rcvdPayload[payloadLen] = 0;
  msgReceived = 1;
}

// Utility to extract header value from headers
String getHeaderValue(String header, String headerName) {
  return header.substring(strlen(headerName.c_str()));
}

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
}
// End list of callback function

// OTA Logic
void execOTA() {
  Serial.println("Connecting to: " + String(host));
  // Connect to S3
  if (wClient.connect(host.c_str(), port)) {
    // Connection Succeed.
    // Fecthing the bin
    Serial.println("Fetching Bin: " + String(bin));

    // Get the contents of the bin file
    wClient.print(String("GET ") + bin + " HTTP/1.1\r\n" +
                  "Host: " + host + "\r\n" +
                  "Cache-Control: no-cache\r\n" +
                  "Connection: close\r\n\r\n");

    // Check what is being sent
    // Serial.print(String("GET ") + bin + " HTTP/1.1\r\n" +
    //              "Host: " + host + "\r\n" +
    //              "Cache-Control: no-cache\r\n" +
    //              "Connection: close\r\n\r\n");

    unsigned long timeout = millis();
    while (wClient.available() == 0) {
      if (millis() - timeout > 5000) {
        Serial.println("Client Timeout !");
        wClient.stop();
        return;
      }
    }

    while (wClient.available()) {
      // read line till /n
      String line = wClient.readStringUntil('\n');
      // remove space, to check if the line is end of headers
      line.trim();
      // Serial.println(String(line));

      // if the the line is empty,
      // this is end of headers
      // break the while and feed the
      // remaining `client` to the
      // Update.writeStream();
      if (!line.length()) {
        //headers ended
        break; // and get the OTA started
      }

      // Check if the HTTP Response is 200
      // else break and Exit Update
      if (line.startsWith("HTTP/1.1")) {
        if (line.indexOf("200") < 0) {
          Serial.println("Got a non 200 status code from server. Exiting OTA Update.");
          break;
        }
      }

      // extract headers here
      // Start with content length
      if (line.startsWith("Content-Length: ")) {
        contentLength = atoi((getHeaderValue(line, "Content-Length: ")).c_str());
        Serial.println("Got " + String(contentLength) + " bytes from server");
      }

      // Next, the content type
      if (line.startsWith("Content-Type: binary/octet-stream")) {
        isValidContentType = true;
      }
    }
  } else {
    // Connect to S3 failed
    // May be try?
    // Probably a choppy network?
    Serial.println("Connection to " + String(host) + " failed. Please check your setup");
    // retry??
    // execOTA();
  }

  // Check what is the contentLength and if content type is `application/octet-stream`
  Serial.println("contentLength : " + String(contentLength) + ", isValidContentType : " + String(isValidContentType));

  // check contentLength and content type
  if (contentLength && isValidContentType) {
    // Check if there is enough to OTA Update
    bool canBegin = Update.begin(contentLength);

    // If yes, begin
    if (canBegin) {
      Serial.println("Begin OTA. This may take 2 - 5 mins to complete. Things might be quite for a while.. Patience!");
      // No activity would appear on the Serial monitor
      // So be patient. This may take 2 - 5mins to complete
      size_t written = Update.writeStream(wClient);

      if (written == contentLength) {
        Serial.println("Written : " + String(written) + " successfully");
      } else {
        Serial.println("Written only : " + String(written) + "/" + String(contentLength) + ". Retry?" );
        // retry??
        // execOTA();
      }

      if (Update.end()) {
        Serial.println("OTA done!");
        if (Update.isFinished()) {
          Serial.println("Update successfully completed. Rebooting.");
          ESP.restart();
        } else {
          Serial.println("Update not finished? Something went wrong!");
        }
      } else {
        Serial.println("Error Occurred. Error #: " + String(Update.getError()));
      }
    } else {
      // not enough space to begin OTA
      // Understand the partitions and
      // space availability
      Serial.println("Not enough space to begin OTA");
      wClient.flush();
    }
  } else {
    Serial.println("There was no content in the response");
    wClient.flush();
  }
}

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

  if (tick % 2 == 0) { // publish to topic every 5seconds
    if (tick >= 3600) {
      // Execute OTA Update for every 1 hour and reset "tick" variable
      tick = 0;
      execOTA();
    }
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
