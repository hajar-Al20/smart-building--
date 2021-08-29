#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define Relay1            D2 //Relays for switching appliances
#define Relay2            D3

#define PIRPIN            D7 //define pir sensor

#define WFlow             D6 //Water Flow Sensor


//Wi-Fi info
const char* ssid = "xxxxx";//your WiFi user Name
const char* password = "xxxxxxxxxx";// your WiFi Password
const char* mqtt_server = "18.116.32.245";  // IP address of Node-RED running on EC2 (globally) 

const char* username = "";
const char* pass = "";
boolean cheak  ;


// you can change all these topics as you whant....
#define sub1 "building/relay" // mqtt topic to send  the relay value
#define waterFlow_topic "Water_Flow2" // mqtt topic to send the Waterr flow sensor value
#define waterFlow_topic_warning "Water_Flow_warning2" // mqtt topic to send the Water Flow warning

// Water Flow Sensor
long currentMillis = 0;
long previousMillis = 0;
int interval = 1000;

float calibrationFactor = 4.5;
volatile byte pulseCount;
byte pulse1Sec = 0;
float flowRate;
unsigned long flowMilliLitres;
unsigned int totalMilliLitres;
float flowLitres;
float totalLitres;
 
void IRAM_ATTR pulseCounter()
{
  pulseCount++;
}


// Connect to WiFi
WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

void setup_wifi() {

  delay(10);
  // Connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}


void callback(char* topic, byte* payload, unsigned int length) { 
  Serial.print("Message arrived ["); 
  Serial.print(topic); 
  Serial.print("] "); 
   
 
 
  if (strstr(topic, sub1)) 
  { 
    for (int i = 0; i < length; i++) { 
      Serial.print((char)payload[i]); 
    } 
    Serial.println(); 
 
    if ((char)payload[0] == '2' ) { 
      digitalWrite(Relay1, LOW);  // Turn the Relay ON (Manuall)
      cheak = false ;  
    } 
 
    
    // Switch on the LED if an 1 was received as first character 
    if ((char)payload[0] == '1' && cheak  ) { 
      digitalWrite(Relay1, LOW);  // Turn the Relay OFF (Automatic)
      // but actually the LED is on.
      //this is because it is active low on the ESP-01 
    } 
    
    else if ((char)payload[0] == '0' && cheak ) { 
      digitalWrite(Relay1, HIGH);  // Turn the Relay ON (Automatic)
    } 
     else if ((char)payload[0] == '3' ) { 
      digitalWrite(Relay1, LOW);  // // Turn the Relay OFF (Manuall)
 
      cheak = true  ; 
 
      delay(5000) ; 
    } 
      
     
  } 
    

  else 
  { 
    Serial.println("unsubscribed topic"); 
  } 
 
 
 
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), username, pass) ) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      // ... and resubscribe
      client.subscribe(sub1);
     
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
   // put your setup code here, to run once:
  pinMode(Relay1, OUTPUT);
  pinMode(PIRPIN , INPUT);
  pinMode(WFlow, INPUT);//Water Flow
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  //Water Flow
  pulseCount = 0;
  flowRate = 0.0;
  flowMilliLitres = 0;
  totalMilliLitres = 0;
  previousMillis = 0;
 
  attachInterrupt(digitalPinToInterrupt(WFlow), pulseCounter, FALLING);
}

void loop() {
 if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  //send pir sensor data to nodered
    long state = digitalRead(PIRPIN);
    if(state == LOW) {
      Serial.println("Motion detected!");
      client.publish("building1/pir", "1");
      delay(1000);
    }
    else {
      Serial.println("NO Motion!");
      client.publish("building1/pir", "0");
      delay(1000);
      }

      
  currentMillis = millis();
  if (currentMillis - previousMillis > interval) 
  {
     pulse1Sec = pulseCount;
    pulseCount = 0;
 //////////////////////////////////////////////////////////////////////////////////////////////
    // Because this loop may not complete in exactly 1 second intervals we calculate
    // the number of milliseconds that have passed since the last execution and use
    // that to scale the output. We also apply the calibrationFactor to scale the output
    // based on the number of pulses per second per units of measure (litres/minute in this case) coming from the sensor.
    
    flowRate = ((1000.0 / (millis() - previousMillis)) * pulse1Sec) / calibrationFactor;
    previousMillis = millis();
    
 ////////////////////////////////////////////////////////////////////////////////////////////
    // Divide the flow rate in litres/minute by 60 to determine how many litres have
    // passed through the sensor in this 1 second interval, then multiply by 1000 to convert to millilitres.
    
    flowMilliLitres = (flowRate / 60) * 1000;
    flowLitres = (flowRate / 60);
 
    // Add the millilitres passed in this second to the cumulative total
    totalMilliLitres += flowMilliLitres;
    totalLitres += flowLitres;


    // water consumtion Warning....
    String warning;
    if(flowRate == 1.00){
      warning = "WARNING!! You are Almost Reaching the Limit";
      Serial.println(warning);
      }
      else if(flowRate > 1.00){
      warning = "WARNING!! You Reached the Limit, the water supply will be cut off..";
      Serial.println(warning);
      }

    
  // PUBLISH to the MQTT Broker (WATER FLOW)
  if (client.publish(waterFlow_topic, String(flowRate).c_str())) {
    Serial.println("Water Flow Rate: ");
    Serial.println(flowRate);
    Serial.println(waterFlow_topic);
    
  }

  // PUBLISH to the MQTT Broker (WATER FLOW WARNING)
  if (client.publish(waterFlow_topic_warning, String(warning).c_str())) {
    Serial.println("Warning! ");
    Serial.println(waterFlow_topic_warning);
    
  }
      
    }
       
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
