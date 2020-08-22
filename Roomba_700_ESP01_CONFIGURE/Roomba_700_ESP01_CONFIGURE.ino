#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <Roomba.h>


//USER CONFIGURED SECTION START//
const char* ssid = "YOUR_WIRELESS_SSID";
const char* password = "YOUR_WIRELESS_SSID";
const char* mqtt_server = "YOUR_MQTT_SERVER_ADDRESS";
const int mqtt_port = YOUR_MQTT_SERVER_PORT;
const char *mqtt_user = "YOUR_MQTT_USERNAME";
const char *mqtt_pass = "YOUR_MQTT_PASSWORD";
const char *mqtt_client_name = "Roomba"; // Client connections can't have the same connection name
//USER CONFIGURED SECTION END//


WiFiClient espClient;
PubSubClient client(espClient);
Roomba roomba(&Serial, Roomba::Baud115200);


// Variables
bool toggle = true;
const int noSleepPin = 2;
bool boot = true;
long battery_Current_mAh = 0;
long battery_Voltage = 0;
long battery_Total_mAh = 0;
long battery_percent = 0;
char battery_percent_send[50];
char battery_Current_mAh_send[50];
uint8_t tempBuf[10];

//Functions

void setup_wifi()
{
  Serial.println("setup_wifi");
  WiFi.mode(WIFI_STA); // Station mode: disable AP broadcast
  WiFi.hostname("Roomba");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("delaying setup_wifi");
    delay(500);
  }
}

void reconnect()
{
  // Loop until we're reconnected
  int retries = 0;
  while (!client.connected())
  {
    Serial.print("reconnect (");
    Serial.print(retries);
    Serial.println(")");
    
    if (retries < 50)
    {
      Serial.print("connecting to MQTT server: ");
      Serial.println(mqtt_client_name);
      
      // Attempt to connect
      if (client.connect(mqtt_client_name, mqtt_user, mqtt_pass, "roomba/status", 0, 0, "Dead somewhere"))
      {
        // Once connected, publish an announcement...
        if (boot == false)
        {
          client.publish("checkIn/roomba", "Reconnected");
        }
        else
        {
          client.publish("checkIn/roomba", "Rebooted");
          boot = false;
        }
        // ... and resubscribe
        client.subscribe("roomba/commands");
      }
      else
      {
        retries++;
        // Wait 5 seconds before retrying
        delay(5000);
      }
    }
    if (retries >= 50)
    {
      Serial.print("Restarting ESP after ");
      Serial.print(retries);
      Serial.println(" wifi connect retries");
      ESP.restart();
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length)
{
  String newTopic = topic;
  payload[length] = '\0';
  String newPayload = String((char *)payload);

  Serial.print("callback - topic: '");
  Serial.print(newTopic);
  Serial.print("', payload: '");
  Serial.print(newPayload);
  Serial.println("'");
  
  if (newTopic == "roomba/commands")
  {
    if (newPayload == "start")
    {
      startCleaning();
    }
    else if (newPayload == "stop")
    {
      stopCleanig();
    }
    else if (newPayload == "home")
    {
      goHome();
    }
    else if (newPayload == "status")
    {
      sendInfoRoomba();
    }
  }
}


void startCleaning()
{
  Serial.println("startCleaning");
  awake();
  Serial.write(128);
  delay(50);

  Serial.write(131);
  delay(50);

  Serial.write(135);
  client.publish("roomba/status", "Cleaning");
}

void stopCleanig()
{
  Serial.println("stopCleaning");
  Serial.write(128);
  delay(50);

  Serial.write(135);
  client.publish("roomba/status", "Halted");
}

void goHome()
{
  Serial.println("goHome");
  awake();
  Serial.write(128);
  delay(50);

  Serial.write(131);
  delay(50);

  Serial.write(143);
  client.publish("roomba/status", "Returning");
}

void sendInfoRoomba()
{
  Serial.println("sendInfoRoomba");
  awake();
  roomba.start();
  roomba.getSensors(21, tempBuf, 1);
  battery_Voltage = tempBuf[0];
  delay(50);

  roomba.getSensors(25, tempBuf, 2);
  battery_Current_mAh = tempBuf[1] + 256 * tempBuf[0];
  delay(50);

  roomba.getSensors(26, tempBuf, 2);
  battery_Total_mAh = tempBuf[1] + 256 * tempBuf[0];

  if (battery_Total_mAh != 0)
  {
    int nBatPcent = 100 * battery_Current_mAh / battery_Total_mAh;
    String temp_str2 = String(nBatPcent);
    temp_str2.toCharArray(battery_percent_send, temp_str2.length() + 1); //packaging up the data to publish to mqtt
    client.publish("roomba/battery", battery_percent_send);
  }
  else if (battery_Total_mAh == 0)
  {
    client.publish("roomba/battery", "NO DATA");
  }

  String temp_str = String(battery_Voltage);
  temp_str.toCharArray(battery_Current_mAh_send, temp_str.length() + 1); //packaging up the data to publish to mqtt
  client.publish("roomba/charging", battery_Current_mAh_send);
}

void awake()
{
  Serial.println("awake");
  digitalWrite(noSleepPin, HIGH);
  delay(1000);

  digitalWrite(noSleepPin, LOW);
  delay(1000);

  digitalWrite(noSleepPin, HIGH);
  delay(1000);

  digitalWrite(noSleepPin, LOW);
}


void setup()
{
  pinMode(noSleepPin, OUTPUT);
  digitalWrite(noSleepPin, HIGH);
  Serial.begin(115200);
  Serial.println("setup");
  Serial.write(129);
  delay(50);

  Serial.write(11);
  delay(50);

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  client.publish("roomba/status", "Initialized");
}

void loop()
{
  delay(1000);

  if (!client.connected())
  {
    reconnect();
  }
  client.loop();
}
