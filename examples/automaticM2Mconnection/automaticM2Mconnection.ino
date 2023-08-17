/*
 * This sketch connects two devices automatically the first time it runs, exchanging MAC address and encryption key
 * 
 * After this it will store and re-use the details from the first connection so should connect more quickly
 * 
 * Once connected it just sends a random number from one node to the other
 * 
 * If you press the optional pairing button it will re-pair again
 * 
 * This demonstrates the basic functions of the library, including (optional) callbacks, how to check data is added and sent
 * 
 */
#include <m2mDirect.h>
//#include "credentials.h"

#if defined(ESP8266)
String name = "Device-" + String(ESP.getChipId(),HEX);  //This will set a name (optional) for the device
#elif defined(ESP32)
String name = "Device-" + String(uint32_t(ESP.getEfuseMac()),HEX);  //This will set a name (optional) for the device
#endif

bool connected = false; //Variable to show if currently connected and avoid 'polling'
uint32_t lastSend = 0;  //Used to send data periodically without delay()

/*
 * 
 * This function is called when the two devices connect
 * 
 */
void onPaired()
{
  if(m2m.remoteNameSet() == true)
  {
    Serial.print(F("\nPaired with device: "));
    Serial.print(m2m.remoteName());
  }
  else
  {
    Serial.print(F("\nPaired"));
  }
}

/*
 * 
 * This function is called when the two devices connect
 * 
 */
void onConnected()
{
  connected = true;
  if(m2m.remoteNameSet() == true)
  {
    Serial.print(F("\nConnected to device: "));
    Serial.print(m2m.remoteName());
  }
  else
  {
    Serial.print(F("\nConnected"));
  }
}

/*
 * 
 * This function is called when the two devices disconnect
 * 
 */
void onDisconnected()
{
  connected = false;
  if(m2m.remoteNameSet() == true)
  {
    Serial.print(F("\nDisconnected from device: "));
    Serial.print(m2m.remoteName());
  }
  else
  {
    Serial.print(F("\nDisconnected"));
  }
}
/*
 * 
 * This function is called when this device receives data
 * 
 */
void onMessageReceived()
{
  Serial.print(F("\nReceived message "));
  if(m2m.dataAvailable() > 0) //dataAvailable is the number of fields of data in a message
  {
    if(m2m.nextDataType() == m2m.DATA_UINT32_T) //nextDataType means you can check what you are trying to retrieve before doing so
    {
      uint32_t receivedData = 0;
      if(m2m.retrieveReceivedData(&receivedData)) //You must pass by reference the variable to retrieve the data into
      {
        Serial.print(F("data received: "));
        Serial.print(receivedData);
        Serial.print(F(" (link quality: "));
        Serial.print(m2m.linkQuality(),HEX);  //Link quality is expressed as MSB 32-bit time series of success/fail. Higher is better
        Serial.print(')');
        if(m2m.dataAvailable() > 0) //dataAvailable is the number of fields of data in a message
        {
          m2m.clearReceivedMessage();  //This is not necessary if ALL the data from the message has been retrieved and is only here as an example
        }
      }
      else
      {
        Serial.print(F("unable to retrieve received data"));
        m2m.clearReceivedMessage();  //Clear the received message to wait for the next message
      }
    }
    else
    {
      Serial.print(F("unable to retrieve received data, data wrong type in message"));
      m2m.clearReceivedMessage();  //Clear the received message to wait for the next message
    }
  }
  else
  {
    Serial.print(F("\nNo data in message"));
    m2m.clearReceivedMessage();  //Clear the received message to wait for the next message
  }
}


void setup()
{
  Serial.begin(115200); //Start the serial interface for debug
  delay(500); //Give some time for the Serial Monitor to come online
  #ifdef APSSID
  WiFi.mode(WIFI_STA);
  Serial.printf("\nConnecting to \"%s\" with PSK \"%s\"", APSSID, APPSK);
  WiFi.begin(APSSID, APPSK);
  while(WiFi.status() != WL_CONNECTED && millis() < 3e4) //Spend 30s trying to connect to WiFi
  {
    delay(500);
    Serial.print(".");
  }
  if(WiFi.status() == WL_CONNECTED)
  {
    Serial.print(F("connected, IP address:"));
    Serial.print(WiFi.localIP());
    Serial.print(F(" RSSI: "));
    Serial.print(WiFi.RSSI());
    Serial.print(F(" channel: "));
    Serial.println(WiFi.channel());
  }
  else
  {
    Serial.println(F("not connected"));
  }
  #endif
  m2m.debug(Serial);  //Tell the library to use Serial for debug output
  m2m.pairingButtonGpio(0); //Enable the pairing button on GPIO 0
  #ifdef LED_BUILTIN
  m2m.indicatorGpio(LED_BUILTIN);  //Enable the indicator LED
  #endif
  m2m.localName(name);  //Set the name of the device
  m2m.setPairedCallback(onPaired);  //Set the 'paired' callback created above
  m2m.setConnectedCallback(onConnected);  //Set the 'connected' callback created above
  m2m.setDisconnectedCallback(onDisconnected);  //Set the 'disconnected' callback created above
  m2m.setMessageReceivedCallback(onMessageReceived);  //Set the 'message received' callback created above
  m2m.begin();  //Start the M2M connection
}

void loop()
{
  m2m.housekeeping(); //Maintain the M2M connection
  if(connected == true) //Only send data if connected
  {
    if(millis() - lastSend > 10000) //Send data every 10s
    {
      lastSend = millis(); //Send the current 'uptime' as test data
      if(m2m.addData(lastSend))
      {
        Serial.print(F("\nSending data "));
        Serial.print(lastSend);
        if(m2m.sendMessage()) //Send it immediately. Note this will 'block' for a short while, until the send is confirmed (or not)
        {
          Serial.print(F(" OK"));
        }
        else
        {
          Serial.print(F(" failed"));
        }
        Serial.print(F(" (link quality:"));
        Serial.print(m2m.linkQuality(),HEX);
        Serial.print(')');
      }
      else
      {
        Serial.print(F("\nUnable to add data "));
        Serial.print(lastSend);
      }
    }
  }
}
