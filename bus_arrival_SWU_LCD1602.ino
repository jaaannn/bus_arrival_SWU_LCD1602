#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

#include <ArduinoJson.h>

#include <LiquidCrystal.h>

// Timing in milliseconds
unsigned long timerDelay = 30000;

// Wifi Config
#define STASSID "XXX"
#define STAPSK "XXX"

//SWU API - https://api.swu.de/mobility/
#define SWU_API_BASE_URL "https://api.swu.de/mobility/v1/stop/passage/Departures"
#define SWU_API_STOP_NUMBER 9999
#define SWU_API_STOP_POINT_NUMBER 1


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
    lcd.clear();
    lcd.print("Connecting...");
  }

  Serial.println("Connection established!");  
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());  
  
  lcd.clear();
  lcd.print("Connected! IP:");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
  delay(5000);
}

// Copies the next two departures for the given stop into the passed next_departures[2][3].
// Returns http response code
int get_data_from_SWU(String (&next_departures)[2][3]) {

  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  client->setInsecure();

  HTTPClient https;
  
  String api_url = String(SWU_API_BASE_URL) + "?Limit=" + String(6) + "&StopNumber=" + String(SWU_API_STOP_NUMBER);

  Serial.print("[HTTPS] begin...\n");
  if (https.begin(*client, api_url)) {

    Serial.print("[HTTPS] GET...\n");
    // start connection and send HTTP header
    int httpCode = https.GET();

    if (httpCode == HTTP_CODE_OK) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
      // file found at server
      String payload = https.getString();

      // Handle Json payload and return next 2 Stops
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
          
          next_departures[i][0] = RouteName;
          next_departures[i][1] = DepartureDirectionText;
          next_departures[i][2] = DepartureCountdownInMin;

          i++;
        }
      }
      // If less then 2 arrivals where returned by the API, fill the rest of the array with empty strings.
      while(i <= 1){
        next_departures[i][0] = "";
        next_departures[i][1] = "";
        next_departures[i][2] = "";         
        i++;
      }
    } 
    else {
      Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
    }
    https.end();
    return httpCode;
  } 
  else {
    Serial.printf("[HTTPS] Unable to connect\n");

    return 9999;
  }
}

void print_bus_depature_on_lcd(String next_departures[2][3]){
      lcd.clear();
      for(int row = 0; row<=1; row++){
        String lineNumber = next_departures[row][0];
        String destination = next_departures[row][1];
        String time = next_departures[row][2];

        lcd.setCursor((2-lineNumber.length()), row);
        lcd.print(lineNumber);
        lcd.setCursor(3, row);
        if(destination.length() >= 10){
          lcd.print(destination.substring(0,9));
          lcd.print(".");
        }
        else{
          lcd.print(destination);
        }
        
        lcd.setCursor((16 - time.length()), row);
        lcd.print(time);
      }
}

void print_http_error(int http_code){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("HTTP-GET failed");
    lcd.setCursor(0,1);
    String error_message = "Error: " + String(http_code);
    lcd.print(error_message);
}

void print_wifi_disconnect(){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("No WiFi!");
    lcd.setCursor(0,1);
    lcd.print("Please Restart");
}

void loop() {
  if(WiFi.status()== WL_CONNECTED){
    String next_departures[2][3];
    int http_code = get_data_from_SWU(next_departures);
    if(http_code == HTTP_CODE_OK){
      print_bus_depature_on_lcd(next_departures);
    }
    else{
      print_http_error(http_code);
    }
  } 
  else{
    print_wifi_disconnect();
  }
  delay(timerDelay);
}
