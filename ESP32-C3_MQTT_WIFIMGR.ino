#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager

#include <PubSubClient.h> //https://github.com/knolleary/pubsubclient/releases/tag/v2.8
#include <ArduinoJson.h>  //https://arduinojson.org/?utm_source=meta&utm_medium=library.properties
#include "Freenove_WS2812_Lib_for_ESP32.h" //https://github.com/Freenove/Freenove_WS2812_Lib_for_ESP32
#include <Preferences.h>

//#define BROKER "broker.hivemq.com"
#define BROKER "test.mosquitto.org"
//#define BROKER "broker.emqx.io"

#define MQTT_TOPIC_IN "esp32c3/led"
#define MQTT_TOPIC_OUT "esp32c3/button"

#define BUTTON 9
#define APBUTTON 19 //access point request button

#define LEDS_COUNT  1
#define LEDS_PIN  8
#define CHANNEL   0

void getPreferences();
void mqttSendButtonState(bool state);
void mqtt_reconnect();
void callback(char* topic, byte* payload, unsigned int length);

//LEDs
Freenove_ESP32_WS2812 strip = Freenove_ESP32_WS2812(LEDS_COUNT, LEDS_PIN, CHANNEL, TYPE_GRB);

//MQTT
WiFiClient espClient;
PubSubClient client(espClient);

Preferences preferences;

static volatile bool buttonStatus=false;

void setup() {
    WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
    // it is a good practice to make sure your code sets wifi mode how you want it.

    // put your setup code here, to run once:
    Serial.begin(115200);

    //ws2812
    strip.begin();
    strip.setBrightness(5);

    strip.setLedColorData(0, 0, 0, 255); //intensity rgb
    strip.show();

    //Next is to setup push button
    pinMode(BUTTON, INPUT_PULLUP);  

    pinMode(APBUTTON, INPUT_PULLUP);

    
    //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wm;

    // reset settings - wipe stored credentials for testing
    // these are stored by the esp library
    //wm.resetSettings();

    // Automatically connect using saved credentials,
    // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
    // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
    // then goes into a blocking loop awaiting configuration and will return success result

    bool res;
    res = wm.autoConnect(); // auto generated AP name from chipid
    // res = wm.autoConnect("AutoConnectAP"); // anonymous ap
    // res = wm.autoConnect("AutoConnectAP","password"); // password protected ap
    

    if(!res) {
        Serial.println("Failed to connect");
        // ESP.restart();
    } 
    else {
        //if you get here you have connected to the WiFi    
        Serial.println("Connected to WiFi");
    }
    

    //mqtt
    client.setServer(BROKER, 1883);
    client.setCallback(callback);
  
    mqttSendButtonState(buttonStatus);

}

void loop() {
    
    
    static bool last_state=false;


  if ((WiFi.status() == WL_CONNECTED))
  {

  // put your main code here, to run repeatedly:
  if (!client.connected()) {
    mqtt_reconnect();     //TODO: - can get stuck here 
  } 
  client.loop();

  //digitalRead(BUTTON) ? buttonStatus=true : buttonStatus=false;
  digitalRead(BUTTON) ? buttonStatus=false : buttonStatus=true; //active lo
 
  //If we have a new state we need to change update and push the new state
  if(last_state!=buttonStatus){
    last_state=buttonStatus;
    mqttSendButtonState(buttonStatus);
    buttonStatus ? Serial.println("TRUE") : Serial.println("FALSE");
    
  }

  }
  else
  {
  }
}


void getPreferences(){

      // Open Preferences with my-app namespace. Each application module, library, etc
  // has to use a namespace name to prevent key name collisions. We will open storage in
  // RW-mode (second parameter has to be false).
  // Note: Namespace name is limited to 15 chars.
  //credentials {
  //ssid: "your_ssid"
  //pass: "your_pass"
  //}

  preferences.begin("credentials", false);
  
  delay(10);
  Serial.println();
  Serial.println();
  Serial.println("Startup");

  // Read preferences for ssid and pass
  Serial.println("Reading preferences");

  // Get the ssid value, if the key does not exist, return a default value of ""
  // Note: Key name is limited to 15 chars.
    
  String psid = preferences.getString("ssid", "");
  
  Serial.println();
  Serial.print("SSID: ");
  Serial.println(psid);
  //Serial.println("Reading Preferences pass");

  String ppass = preferences.getString("pass", "");
  Serial.print("PASS: ");
  Serial.println(ppass);


  WiFi.begin(psid.c_str(), ppass.c_str());

  String pbroker = preferences.getString("broker", "");
  Serial.print("BROKER: ");
  Serial.println(pbroker);
  
  preferences.end();


}

void mqtt_reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP32-C3";
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe(MQTT_TOPIC_IN);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void mqttSendButtonState(bool state){
  char buffer[256];
  StaticJsonDocument<32> doc;
  doc["buttonStatus"] = (bool)state; 
  size_t n = serializeJson(doc, buffer);
  client.publish(MQTT_TOPIC_OUT, (const uint8_t*)buffer, n,true);

}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  StaticJsonDocument<0> filter;
  filter.set(true);
  StaticJsonDocument<64> doc;

  DeserializationError error = deserializeJson(doc, payload, length, DeserializationOption::Filter(filter));

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }

  
  
  Serial.println((uint8_t)doc["bright"]);
  Serial.println((uint8_t)doc["red"]);
  Serial.println((uint8_t)doc["green"]);
  Serial.println((uint8_t)doc["blue"]);

  strip.setBrightness(doc["bright"]);

  strip.setLedColorData(0, doc["red"], doc["green"], doc["blue"]);
  strip.show();
}
