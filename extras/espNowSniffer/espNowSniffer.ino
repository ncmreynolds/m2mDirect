/*
 * This is a simple ESP-Now scanner
 * 
 * It shows all ESP-Now broadcast packets received on the Serial port for the configured channel
 * 
 * This is useful to watch pairing happen
 * 
 */

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  extern "C" {
      #include <espnow.h>
    #include <user_interface.h>
  }
  #define ESP_OK 0
#elif defined(ESP32)
  #include <WiFi.h>
  #include <esp_system.h>
  extern "C" {
    #include <esp_now.h>
  }
#endif

uint8_t channel = 1;

void setup() {
  Serial.begin(115200);
  delay(500);
  //Channel 14 is only usable in Japan
  if (channel > 13)
  {
    wifi_country_t wiFiCountryConfiguration;
    wiFiCountryConfiguration.cc[0] = 'J';
    wiFiCountryConfiguration.cc[1] = 'P';
    wiFiCountryConfiguration.cc[2] = '\0';
    wiFiCountryConfiguration.schan = 1;
    wiFiCountryConfiguration.nchan = 14;
    wiFiCountryConfiguration.policy = WIFI_COUNTRY_POLICY_MANUAL;
    if (wifi_set_country(&wiFiCountryConfiguration) == false) {
      Serial.println(F("Unable to set channel to 14"));
    }
  }
  //Basic WiFi
  WiFi.mode(WIFI_STA);
  //WiFi.channel(channel);
  WiFi.disconnect();
  //ESP-Now
  if(esp_now_init() == ESP_OK)
  {
    Serial.print(F("\nESP-Now initialised on channel "));
    Serial.println(channel);
    if(esp_now_set_self_role(ESP_NOW_ROLE_COMBO) == ESP_OK)
    {
      esp_now_register_recv_cb([](uint8_t *macaddr, uint8_t *message, uint8_t messageLength) {
        Serial.printf("ESP-Now broadcast from %02x:%02x:%02x:%02x:%02x:%02x ",macaddr[0], macaddr[1], macaddr[2], macaddr[3], macaddr[4], macaddr[5]);
        Serial.print(F("data: "));
        for (int i = 0; i < messageLength; i++) {
          Serial.printf("%02x(%c) ",message[i], (message[i] > 32 ? message[i] : '?'));
        }
        Serial.println("");
      });
    }
  }
}

void loop() {
  // put your main code here, to run repeatedly:

}
