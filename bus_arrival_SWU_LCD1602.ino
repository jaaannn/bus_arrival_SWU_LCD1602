#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

#include <ArduinoJson.h>

#include <LiquidCrystal.h>

// Timing in milliseconds
unsigned long lastTime = 0;
unsigned long timerDelay = 30000;

// Wifi Config
#define STASSID "XXX"
#define STAPSK "XXX"

//SWU API - https://api.swu.de/mobility/
#define SWU_API_BASE_URL "https://api.swu.de/mobility/v1/stop/passage/Departures"
#define SWU_API_STOP_NUMBER 1
#define SWU_API_STOP_POINT_NUMBER 2


//LCD
const int rs = D7, en = D6, d4 = D4, d5 = D3, d6 = D1, d7 = D2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);


ESP8266WiFiMulti WiFiMulti;
void setup() {

  Serial.begin(115200);
  // Serial.setDebugOutput(true);

  lcd.begin(16, 2);

  WiFi.hostname("ESP-12F");
  WiFi.mode(WIFI_STA);

  WiFiMulti.addAP(STASSID, STAPSK);
  Serial.println("Connecting to ssid '" STASSID "'");
  // Wait for the Wi-Fi to connect
  while (WiFiMulti.run() != WL_CONNECTED) { 
    delay(1000);
    Serial.println("Connecting...");
  }
  Serial.println("Connection established!");  
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());  
  lcd.print("Connected! IP:");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
}

// Returns the next two stops for the defined bus stop
void get_data_from_SWU(char (&next_departures)[2][3][50]) {

  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  client->setInsecure();

  HTTPClient https;
  
  String api_url = String(SWU_API_BASE_URL) + "?Limit=" + String(6) + "&StopNumber=" + String(SWU_API_STOP_NUMBER);

  Serial.print("[HTTPS] begin...\n");
  if (https.begin(*client, api_url)) {

    Serial.print("[HTTPS] GET...\n");
    // start connection and send HTTP header
    int httpCode = https.GET();

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        String payload = https.getString();

        // Handle Json payload and return next 2 Stops for Direction B
        DynamicJsonDocument doc(2048);
        deserializeJson(doc, payload);
        JsonArray data = doc["StopPassage"]["DepartureData"];

        int i = 0;
        for (JsonVariant item : data) {
          if(item["StopPointNumber"] == SWU_API_STOP_POINT_NUMBER && i <= 1){ 
            String RouteName = item["RouteName"];
            String DepartureDirectionText = item["DepartureDirectionText"];
            short DepartureCountdown = item["DepartureCountdown"];
            String DepartureCountdownInMin = String(DepartureCountdown/60);
            strcpy(next_departures[i][0], RouteName.c_str());
            strcpy(next_departures[i][1], DepartureDirectionText.c_str());
            strcpy(next_departures[i][2], DepartureCountdownInMin.c_str());

            i++;
          }
        }
      }
    } 
    else {
      Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("GET... failed, error:");
      lcd.setCursor(0,1);
      lcd.print(https.errorToString(httpCode).c_str());
    }
      
  https.end();
  } 
  else {
    Serial.printf("[HTTPS] Unable to connect\n");
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("[HTTPS] Unable to connect");
  }
}

void print_bus_depature_on_lcd(const char* lineNumber, const char* destination, const char* time, int row){
      lcd.setCursor((2-strlen(lineNumber)), row);
      lcd.print(lineNumber);
      lcd.setCursor(3, row);
      if(strlen(destination) >= 10){
        lcd.print(String(destination).substring(0,9));
        lcd.print(".");
      }
      else{
        lcd.print(destination);
      }
      lcd.setCursor((16 - strlen(time)), row);
      lcd.print(time);
}

void loop() {
  if ((millis() - lastTime) > timerDelay){
    if(WiFi.status()== WL_CONNECTED){
      char next_departures[2][3][50];
      get_data_from_SWU(next_departures);
      lcd.clear();
      print_bus_depature_on_lcd(next_departures[0][0],next_departures[0][1],next_departures[0][2],0);
      print_bus_depature_on_lcd(next_departures[1][0],next_departures[1][1],next_departures[1][2],1);
    } 
    else{
      Serial.println("WiFi Disconnected!");
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("WiFi Disconnected!");
    }
  lastTime = millis();
  }
}
