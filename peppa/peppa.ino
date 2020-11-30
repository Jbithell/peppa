#include <WiFi.h>
#include <HTTPClient.h>
#include "time.h" //NTP Time
#include <EEPROM.h>
#define EEPROM_SIZE 1

#include ".\config.h"

#define SLOWEST_ROTATION 5000 //The maximum amount of time in ms it can take to do a complete lap
#define UPLOAD_FREQUENCY 3600000

#define SENSOR1 4
#define SENSOR2 5

#define DSTOFFSET 3600

//Internal Variables

volatile int rotationStart = 0;
volatile int awaiting = 1; //Which loop are we currently waiting for

volatile int laps = 0;
volatile int fastestLap = 9999;
volatile unsigned long totalLapTime = 0;

const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.google.com";
const char* ntpServer3 = "time.cloudflare.com";

void disconnectWifi() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("WiFi disconnected");
}
void connectWifi() {
  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
}

int timeMinutes()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return 99;
  }
  return timeinfo.tm_min;
}
int timeHours()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return 99;
  }
  return timeinfo.tm_hour;
}
int timeDay()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return 99;
  }
  return timeinfo.tm_mday;
}
int timeMonth()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return 99;
  }
  return timeinfo.tm_mon;
}

void setup() {
  EEPROM.begin(EEPROM_SIZE);
  
  Serial.begin(9600);
  Serial.println("Booted");

  connectWifi();
  configTime(0, DSTOFFSET, ntpServer1, ntpServer2,ntpServer3);
  disconnectWifi();
  
  pinMode(SENSOR1, INPUT_PULLUP);
  pinMode(SENSOR2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SENSOR1), pulseOne, RISING);
  attachInterrupt(digitalPinToInterrupt(SENSOR2), pulseTwo, RISING);
}
void pulseOne() {
  if (awaiting == 3) { //The lap has just ended - and another one has begin  
    if ((millis()-rotationStart) <= SLOWEST_ROTATION) { //Evaluate the timings of the last lap
      //Store the lap
      int thisTime = millis()-rotationStart;
      if (thisTime < fastestLap) {
        fastestLap = thisTime;
      }
      totalLapTime += thisTime;
      laps++;
      Serial.print("New lap ");
      Serial.println(thisTime);
    }
  }

  //Setup to expect the next pulse
  awaiting = 2;
  rotationStart = millis();
}
void pulseTwo() {
  if (awaiting == 2) {
    awaiting = 3; 
  } else {
    awaiting = 1;
  }
}

void loop() {
  if (timeHours() > 5) { //6am
    if (timeDay() != EEPROM.read(0)) { //The upload for today hasn't been done yet!
      detachInterrupt(digitalPinToInterrupt(SENSOR1));
      detachInterrupt(digitalPinToInterrupt(SENSOR2));
      if (laps > 10 && fastestLap < 9999 && totalLapTime > 0) {
        connectWifi();
        if(WiFi.status()== WL_CONNECTED){
          HTTPClient http;
          
          http.begin(serverName);
          
          http.addHeader("Content-Type", "application/x-www-form-urlencoded");
          // Data to send with HTTP POST
          String httpRequestData = "value1=" + String(totalLapTime) + "&value2=" + String(laps) + "&value3=" + String(fastestLap);           
          // Send HTTP POST request
          int httpResponseCode = http.POST(httpRequestData);
          
          /*
          // If you need an HTTP request with a content type: application/json, use the following:
          http.addHeader("Content-Type", "application/json");
          // JSON data to send with HTTP POST
          String httpRequestData = "{\"value1\":\"" + String(random(40)) + "\",\"value2\":\"" + String(random(40)) + "\",\"value3\":\"" + String(random(40)) + "\"}";
          // Send HTTP POST request
          int httpResponseCode = http.POST(httpRequestData);
          */
         
          if (httpResponseCode != 200) {
            Serial.print("FAIL     HTTP Response code: ");
            Serial.println(httpResponseCode);
          } else {
            Serial.println("Success");
          }
         
          http.end();
        }
      }
  
      EEPROM.write(0, timeDay()); //Set the memory to today
      EEPROM.commit();
      ESP.restart();
    }
  }

  delay(5000);
}
