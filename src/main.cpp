#include <Arduino.h>
#include <HardwareSerial.h>
#include <PubSubClient.h>
#include <WiFi.h>

const char* ssid = "ACTFIBERNET";
const char* password = "act12345";
const char* mqttServer = "mqtt.zig-web.com";
const int mqttPort = 1883;
bool distance_below_1_foot = false;
bool printedMessage = false;
bool messagePrinted = false;
unsigned long start_time = 0; // Time when user comes within 1 foot range
unsigned long end_time = 0;   // Time when valid MQTT data is received

WiFiClient espClient;
PubSubClient client(espClient);

unsigned char TOF_data[32] = {0};  
unsigned char TOF_length = 16;
unsigned char TOF_header[3] {0x57,0x00,0xFF};
unsigned long TOF_system_time = 0;
unsigned long TOF_distance = 0;
unsigned char TOF_status = 0;
unsigned int TOF_signal = 0;
unsigned char TOF_check = 0;

void setup_wifi();
void setup_mqtt();
void printIncomingMQTT(byte* payload, unsigned int length);

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600);
  setup_wifi();
  setup_mqtt();
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  // Convert payload to string
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  if (distance_below_1_foot && !printedMessage && !message.startsWith("301") && !message.startsWith("SOS") && !message.startsWith("STOP")) {
    Serial.print("Message received on topic: ");
    Serial.println(topic);
    Serial.print("Message: ");
    Serial.println(message);
    printedMessage = true;
    end_time = millis(); // Record time when valid MQTT data is received
    unsigned long time_taken = end_time - start_time;
    Serial.print("Time taken: ");
    Serial.print(time_taken);
    Serial.println(" milliseconds");
    String data_with_time = message + "#BEN" + String(time_taken); 
    client.publish("04:e9:e5:16:f6:3d/ben", data_with_time.c_str());
  }
}

void setup_mqtt() {
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback); // Set callback function for incoming messages
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client")) {
      Serial.println("connected");
      client.subscribe("04:e9:e5:16:f6:3d/device");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

bool verifyCheckSum(unsigned char data[], unsigned char len) {
  TOF_check = 0;

  for (int k = 0; k < len - 1; k++) {
    TOF_check += data[k];
  }

  if (TOF_check == data[len - 1]) {
    return true;
  } else {
    return false;
  }
}

void clearSerialBuffer() {
  while (Serial.available() > 0) {
    Serial.read();
  }
  Serial.flush();
}

void loop() {
  delay(50);
  if (Serial2.available() >= 32) {
    for (int i = 0; i < 32; i++) {
      TOF_data[i] = Serial2.read();
    }
    for (int j = 0; j < 16; j++) {
      if ((TOF_data[j] == TOF_header[0] && TOF_data[j + 1] == TOF_header[1] && TOF_data[j + 2] == TOF_header[2]) && (verifyCheckSum(&TOF_data[j], TOF_length))) {
        if (((TOF_data[j + 12]) | (TOF_data[j + 13] << 8)) == 0) {
          Serial.println("Out of range!");
          distance_below_1_foot = false;
          printedMessage = false;
          messagePrinted = false;
        } else {
          TOF_distance = (TOF_data[j + 8]) | (TOF_data[j + 9] << 8) | (TOF_data[j + 10] << 16);
          float distance_feet = TOF_distance * 0.00328084;
          Serial.print("TOF distance is: ");
          Serial.print(distance_feet, 2); // Print with 2 decimal places
          Serial.println(" feet");
          Serial.println(""); 
          
          // Check if distance is below 1 foot
          if (distance_feet < 2) {
            distance_below_1_foot = true;
            if (!start_time) {
              start_time = millis(); // Record time when user comes within 1 foot range
            }
          } else {
            distance_below_1_foot = false;
            printedMessage = false;
            messagePrinted = false;
            start_time = 0; // Reset start_time when user goes out of 1 foot range
          }
        }
        break;
      }
    }
  }
  
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Reset distance_below_1_foot flag after printing MQTT message
  if (printedMessage && !messagePrinted) {
    distance_below_1_foot = false;
    messagePrinted = true;
  }
}
