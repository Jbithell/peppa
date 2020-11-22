#include <WiFi.h>
#include <HTTPClient.h>

#include ".\config.h"

#define SLOWEST_ROTATION 1000 //The maximum amount of time in ms it can take to go from one magnet to the other
#define FASTEST_ROTATION 10
 
volatile int lastRotation1 = 0;
volatile int lastRotation2 = 0;
volatile int awaiting = 1; //Which loop are we currently waiting for
volatile bool lapInProgress = false;

volatile int laps = 0;
volatile int fastestLap = 9999;
volatile unsigned long totalLapTime = 0;

bool uploaded = false;


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
void setup() {
  pinMode(15, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(15), pulse, FALLING);
  Serial.begin(9600);
  Serial.println("Booted");
}
void pulse() {
  if (!lapInProgress) {
      awaiting = 2;
      lastRotation1 = millis();
      lapInProgress = true;
  } else {
    if (awaiting == 2) {
      //We've got the second magnet - time to wait for the 3rd
      if ((millis()-lastRotation1) > SLOWEST_ROTATION || (millis()-lastRotation1) < FASTEST_ROTATION) {
        awaiting = 2;
        lastRotation1 = millis();
        lapInProgress = true;
      } else {
        lastRotation2 = millis();
        awaiting = 3;
      }
      
    } else {
      //The lap has just ended - and another one has begin
      if ((millis()-lastRotation2) > SLOWEST_ROTATION || (lastRotation2-lastRotation1) > SLOWEST_ROTATION || (millis()-lastRotation2) < FASTEST_ROTATION) {
        //Too fast or slow - reject it
      } else {
        //Store the lap
        int thisTime = millis()-lastRotation1;
        if (thisTime < fastestLap) {
          fastestLap = thisTime;
        }
        totalLapTime += thisTime;
        laps++;
      }
      
      awaiting = 2;
      lastRotation1 = millis();
      lapInProgress = true;
      
    }
  }

}
void loop() {
  if (millis() > 21600000 && !uploaded) { //12 hours
    detachInterrupt(digitalPinToInterrupt(15));
    uploaded = true;
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
      
      ESP.restart();
    }
  }
}
