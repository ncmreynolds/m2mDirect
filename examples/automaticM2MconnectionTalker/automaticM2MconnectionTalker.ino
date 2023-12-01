/*
 * This sketch connects two devices automatically the first time it runs, exchanging MAC address and encryption key
 * 
 * After this it will store and re-use the details from the first connection so should connect more quickly
 * 
 * This sketch will send random data of random types, which it also shows on the Serial monitor
 * 
 * If you hold the optional pairing button (usually connected at BOOT/GPIO0 on many dev boards) for 5s it will re-pair again
 * 
 * This demonstrates the basic functions of the library, including (optional) callbacks and how to check data is added and sent
 * 
 */
#include <m2mDirect.h>

#if defined(ESP8266)
  String name = "Device-" + String(ESP.getChipId(),HEX);  //This will set a name (optional) for the device
#elif defined(ESP32)
  String name = "Device-" + String(uint32_t(ESP.getEfuseMac()),HEX);  //This will set a name (optional) for the device
#endif
/*
 * 
 * This function is called when this device starts pairing
 * 
 */
void onPairStart()
{
  Serial.print(F("\n\rStarting pairing"));
}
/*
 * 
 * This function is called when the two devices connect
 * 
 */
void onPaired()
{
  if(m2m.remoteNameSet() == true)
  {
    Serial.print(F("\n\rPaired with device: "));
    Serial.print(m2m.remoteName());
  }
  else
  {
    Serial.print(F("\n\rPaired"));
  }
}
/*
 * 
 * This function is called when the two devices connect
 * 
 */
void onConnected()
{
  if(m2m.remoteNameSet() == true)
  {
    Serial.print(F("\n\rConnected to device: "));
    Serial.print(m2m.remoteName());
  }
  else
  {
    Serial.print(F("\n\rConnected"));
  }
}

/*
 * 
 * This function is called when the two devices disconnect
 * 
 */
void onDisconnected()
{
  if(m2m.remoteNameSet() == true)
  {
    Serial.print(F("\n\rDisconnected from device: "));
    Serial.print(m2m.remoteName());
  }
  else
  {
    Serial.print(F("\n\rDisconnected"));
  }
}
/*
 * 
 * This function is called when this device receives data
 * 
 */
void onMessageReceived()
{
  Serial.printf(PSTR("\n\rReceived message, link quality: %02x\ discarded"),m2m.linkQuality());
  m2m.clearReceivedMessage();  //Clear the received message to wait for the next message
}
uint32_t lastSend = 0;
uint8_t fieldsToSend = 0;
uint8_t typeToSend = 0;
char shortTestStr[] = "shortStr";
char mediumTestStr[] = "Medium length test null terminated char array";
char utf8TestStr[] = "Y [ˈʏpsilɔn], Yen [jɛn], Yoga [ˈjoːgɑ]";

void setup()
{
  Serial.begin(115200); //Start the serial interface for debug
  delay(500); //Give some time for the Serial Monitor to come online
  //m2m.debug(Serial);  //Tell the library to use Serial for debug output
  m2m.pairingButtonGpio(0); //Enable the pairing button on GPIO 0 which is often available on ESP board as the 'program' button
  #ifdef LED_BUILTIN
    m2m.indicatorGpio(LED_BUILTIN);  //Enable the indicator LED
  #endif
  m2m.localName(name);  //Set the name of the device
  m2m.setPairingCallback(onPairStart);  //Set the 'pairing' callback created above
  m2m.setPairedCallback(onPaired);  //Set the 'paired' callback created above
  m2m.setConnectedCallback(onConnected);  //Set the 'connected' callback created above
  m2m.setDisconnectedCallback(onDisconnected);  //Set the 'disconnected' callback created above
  m2m.setMessageReceivedCallback(onMessageReceived);  //Set the 'message received' callback created above
  m2m.setAutomaticTxPower(true); //Enable automatic adjustment of transmit power (default, use false to disable this)
  m2m.begin(1);  //Start the M2M connection
}

void loop()
{
  m2m.housekeeping(); //Maintain the M2M connection
  if(millis() - lastSend > 10000)
  {
    lastSend = millis();
    if(m2m.connected())
    {
      fieldsToSend = random(1,16);
      Serial.printf(PSTR("\r\nSending message with %u fields, link quality: %02x"),fieldsToSend , m2m.linkQuality());
      while(fieldsToSend > 0)
      {
        //typeToSend = 0;
        typeToSend = random(0,13);
        switch (typeToSend)
        {
          case 0:
            addRandomBool();
          break;
          case 1:
            addRandomUint8_t();
          break;
          case 2:
            addRandomUint16_t();
          break;
          case 3:
            addRandomUint32_t();
          break;
          case 4:
            addRandomUint64_t();
          break;
          case 5:
            addRandomInt8_t();
          break;
          case 6:
            addRandomInt16_t();
          break;
          case 7:
            addRandomInt32_t();
          break;
          case 8:
            addRandomInt64_t();
          break;
          case 9:
            addRandomFloat();
          break;
          case 10:
            addRandomDouble();
          break;
          case 11:
            addRandomChar();
          break;
          case 12:
            addRandomStr();
          break;
        }
      }
      if(m2m.sendMessage())
      {
        #ifdef DEBUG
          Serial.print(F(" sent"));
        #endif
      }
      else
      {
        #ifdef DEBUG
          Serial.print(F(" send failed"));
        #endif
      }
    }
  }
}

void addRandomBool()
{
  if(random(0,2) == 0)
  {
    bool valueToSend = randomBool();
    Serial.print(F("\r\n\tbool: "));
    Serial.print(valueToSend == true ? "true" : "false");
    if(m2m.add(valueToSend) == false)
    {
      Serial.print(F(" failed"));
    }
  }
  else  //Send an array
  {
    uint8_t arraySize = random(2,9);
    bool valuesToSend[arraySize];
    Serial.print(F("\r\n\tbool["));
    Serial.print(arraySize);
    Serial.print(F("]: "));
    for(uint8_t index = 0; index < arraySize; index++)
    {
      valuesToSend[index] = randomBool();
      Serial.print(valuesToSend[index] == true ? "true": "false");
      Serial.print(' ');
    }
    if(m2m.add(valuesToSend, arraySize) == false)
    {
      Serial.print(F("failed"));
    }
  }
  fieldsToSend--;
}
bool randomBool()
{
  if(random(0,2) == 0)
  {
    return true;
  }
  return false;
}
void addRandomUint8_t()
{
  if(random(0,2) == 0)  //Send a single item
  {
    uint8_t valueToSend = randomUint8_t();
    Serial.print(F("\r\n\tuint8_t: "));
    Serial.print(valueToSend);
    if(m2m.add(valueToSend) == false)
    {
      Serial.print(F(" failed"));
    }
  }
  else  //Send an array
  {
    uint8_t arraySize = random(2,9);
    uint8_t valuesToSend[arraySize];
    Serial.print(F("\r\n\tuint8_t["));
    Serial.print(arraySize);
    Serial.print(F("]: "));
    for(uint8_t index = 0; index < arraySize; index++)
    {
      valuesToSend[index] = randomUint8_t();
      Serial.print(valuesToSend[index]);
      Serial.print(' ');
    }
    if(m2m.add(valuesToSend, arraySize) == false)
    {
      Serial.print(F("failed"));
    }
  }
  fieldsToSend--;
}
uint8_t randomUint8_t()
{
  return (uint8_t)random(0,256);
}
void addRandomUint16_t()
{
  if(random(0,2) == 0)  //Send a single item
  {
    uint16_t valueToSend = randomUint16_t();
    Serial.print(F("\r\n\tuint16_t: "));
    Serial.print(valueToSend);
    if(m2m.add(valueToSend) == false)
    {
      Serial.print(F(" failed"));
    }
  }
  else  //Send an array
  {
    uint8_t arraySize = random(2,9);
    uint16_t valuesToSend[arraySize];
    Serial.print(F("\r\n\tuint16_t["));
    Serial.print(arraySize);
    Serial.print(F("]: "));
    for(uint8_t index = 0; index < arraySize; index++)
    {
      valuesToSend[index] = randomUint16_t();
      Serial.print(valuesToSend[index]);
      Serial.print(' ');
    }
    if(m2m.add(valuesToSend, arraySize) == false)
    {
      Serial.print(F("failed"));
    }
  }
  fieldsToSend--;
}
uint16_t randomUint16_t()
{
  return random(0,65536);
}
void addRandomUint32_t()
{
  if(random(0,2) == 0)  //Send a single item
  {
    uint32_t valueToSend3 = randomUint32_t();
    Serial.print(F("\r\n\tuint32_t: "));
    Serial.print(valueToSend3);
    if(m2m.add(valueToSend3) == false)
    {
      Serial.print(F(" failed"));
    }
  }
  else  //Send an array
  {
    uint8_t arraySize = random(2,9);
    uint32_t valuesToSend[arraySize];
    Serial.print(F("\r\n\tuint32_t["));
    Serial.print(arraySize);
    Serial.print(F("]: "));
    for(uint8_t index = 0; index < arraySize; index++)
    {
      valuesToSend[index] = randomUint32_t();
      Serial.print(valuesToSend[index]);
      Serial.print(' ');
    }
    if(m2m.add(valuesToSend, arraySize) == false)
    {
      Serial.print(F("failed"));
    }
  }
  fieldsToSend--;
}
uint32_t randomUint32_t()
{
  uint32_t randomValue = random(0,65536);
  randomValue += (uint32_t)random(0,65536)<<16;
  return randomValue;
}
void addRandomUint64_t()
{
  if(random(0,2) == 0)  //Send a single item
  {
    uint64_t valueToSend = randomUint64_t();
    Serial.print(F("\r\n\tuint64_t: "));
    Serial.print(valueToSend);
    if(m2m.add(valueToSend) == false)
    {
      Serial.print(F(" failed"));
    }
  }
  else  //Send an array
  {
    uint8_t arraySize = random(2,9);
    uint64_t valuesToSend[arraySize];
    Serial.print(F("\r\n\tuint64_t["));
    Serial.print(arraySize);
    Serial.print(F("]: "));
    for(uint8_t index = 0; index < arraySize; index++)
    {
      valuesToSend[index] = randomUint64_t();
      Serial.print(valuesToSend[index]);
      Serial.print(' ');
    }
    if(m2m.add(valuesToSend, arraySize) == false)
    {
      Serial.print(F("failed"));
    }
  }
  fieldsToSend--;
}
uint64_t randomUint64_t()
{
  uint64_t randomValue = random(0,65536);
  randomValue += (uint64_t)random(0,65536)<<16;
  randomValue += (uint64_t)random(0,65536)<<32;
  randomValue += (uint64_t)random(0,65536)<<48;
  return randomValue;
}
void addRandomInt8_t()
{
  if(random(0,2) == 0)  //Send a single item
  {
    int8_t valueToSend = randomInt8_t();
    Serial.print(F("\r\n\tint8_t: "));
    Serial.print(valueToSend);
    if(m2m.add(valueToSend) == false)
    {
      Serial.print(F(" failed"));
    }
  }
  else  //Send an array
  {
    uint8_t arraySize = random(2,9);
    int8_t valuesToSend[arraySize];
    Serial.print(F("\r\n\tint8_t["));
    Serial.print(arraySize);
    Serial.print(F("]: "));
    for(uint8_t index = 0; index < arraySize; index++)
    {
      valuesToSend[index] = randomInt8_t();
      Serial.print(valuesToSend[index]);
      Serial.print(' ');
    }
    if(m2m.add(valuesToSend, arraySize) == false)
    {
      Serial.print(F("failed"));
    }
  }
  fieldsToSend--;
}
int8_t randomInt8_t()
{
  return random(-127,128);
}
void addRandomInt16_t()
{
  if(random(0,2) == 0)  //Send a single item
  {
    int16_t valueToSend = randomInt16_t();
    Serial.print(F("\r\n\tint16_t: "));
    Serial.print(valueToSend);
    if(m2m.add(valueToSend) == false)
    {
      Serial.print(F(" failed"));
    }
  }
  else  //Send an array
  {
    uint8_t arraySize = random(2,9);
    int16_t valuesToSend[arraySize];
    Serial.print(F("\r\n\tint16_t["));
    Serial.print(arraySize);
    Serial.print(F("]: "));
    for(uint8_t index = 0; index < arraySize; index++)
    {
      valuesToSend[index] = randomInt16_t();
      Serial.print(valuesToSend[index]);
      Serial.print(' ');
    }
    if(m2m.add(valuesToSend, arraySize) == false)
    {
      Serial.print(F("failed"));
    }
  }
  fieldsToSend--;
}
int16_t randomInt16_t()
{
  return random(-32767,32768);  
}
void addRandomInt32_t()
{
  if(random(0,2) == 0)  //Send a single item
  {
    int32_t valueToSend = randomInt32_t();
    Serial.print(F("\r\n\tint32_t: "));
    Serial.print(valueToSend);
    if(m2m.add(valueToSend) == false)
    {
      Serial.print(F(" failed"));
    }
  }
  else  //Send an array
  {
    uint8_t arraySize = random(2,9);
    int32_t valuesToSend[arraySize];
    Serial.print(F("\r\n\tint32_t["));
    Serial.print(arraySize);
    Serial.print(F("]: "));
    for(uint8_t index = 0; index < arraySize; index++)
    {
      valuesToSend[index] = randomInt32_t();
      Serial.print(valuesToSend[index]);
      Serial.print(' ');
    }
    if(m2m.add(valuesToSend, arraySize) == false)
    {
      Serial.print(F("failed"));
    }
  }
  fieldsToSend--;
}
int32_t randomInt32_t()
{
  int32_t randomValue = random(-32767,32768);
  randomValue += constrain(randomValue, -1, 1)*(int32_t)random(0,32767)<<16;
  return randomValue;
}
void addRandomInt64_t()
{
  if(random(0,2) == 0)  //Send a single item
  {
    int64_t valueToSend = randomInt64_t();
    Serial.print(F("\r\n\tint64_t: "));
    Serial.print(valueToSend);
    if(m2m.add(valueToSend) == false)
    {
      Serial.print(F(" failed"));
    }
  }
  else  //Send an array
  {
    uint8_t arraySize = random(2,9);
    int64_t valuesToSend[arraySize];
    Serial.print(F("\r\n\tint64_t["));
    Serial.print(arraySize);
    Serial.print(F("]: "));
    for(uint8_t index = 0; index < arraySize; index++)
    {
      valuesToSend[index] = randomInt64_t();
      Serial.print(valuesToSend[index]);
      Serial.print(' ');
    }
    if(m2m.add(valuesToSend, arraySize) == false)
    {
      Serial.print(F("failed"));
    }
  }
  fieldsToSend--;
}
int64_t randomInt64_t()
{
  int64_t randomValue = random(-32767,32768);
  randomValue += constrain(randomValue, -1, 1)*(int32_t)random(0,32767)<<16;
  randomValue += constrain(randomValue, -1, 1)*(int32_t)random(0,32767)<<32;
  randomValue += constrain(randomValue, -1, 1)*(int32_t)random(0,32767)<<48;
  return randomValue;
}
void addRandomFloat()
{
  if(random(0,2) == 0)  //Send a single item
  {
    float valueToSend = randomFloat();
    Serial.print(F("\r\n\tfloat: "));
    Serial.print(valueToSend);
    if(m2m.add(valueToSend) == false)
    {
      Serial.print(F(" failed"));
    }
  }
  else  //Send an array
  {
    uint8_t arraySize = random(2,9);
    float valuesToSend[arraySize];
    Serial.print(F("\r\n\tfloat["));
    Serial.print(arraySize);
    Serial.print(F("]: "));
    for(uint8_t index = 0; index < arraySize; index++)
    {
      valuesToSend[index] = randomFloat();
      Serial.print(valuesToSend[index]);
      Serial.print(' ');
    }
    if(m2m.add(valuesToSend, arraySize) == false)
    {
      Serial.print(F("failed"));
    }
  }
  fieldsToSend--;
}
float randomFloat()
{
  return (float)random(1,255) / (float)random(1,32);
}
void addRandomDouble()
{
  if(random(0,2) == 0)  //Send a single item
  {
    double valueToSend = randomDouble();
    Serial.print(F("\r\n\tdouble: "));
    Serial.print(valueToSend);
    if(m2m.add(valueToSend) == false)
    {
      Serial.print(F(" failed"));
    }
  }
  else  //Send an array
  {
    uint8_t arraySize = random(2,9);
    double valuesToSend[arraySize];
    Serial.print(F("\r\n\tdouble["));
    Serial.print(arraySize);
    Serial.print(F("]: "));
    for(uint8_t index = 0; index < arraySize; index++)
    {
      valuesToSend[index] = randomDouble();
      Serial.print(valuesToSend[index]);
      Serial.print(' ');
    }
    if(m2m.add(valuesToSend, arraySize) == false)
    {
      Serial.print(F("failed"));
    }
  }
  fieldsToSend--;
}
double randomDouble()
{
  return (double)random(1,255) / (double)random(1,32);
}
void addRandomChar()
{
  if(random(0,2) == 0)  //Send a single item
  {
    char valueToSend = randomChar();
    Serial.print(F("\r\n\tchar: '"));
    Serial.print(valueToSend);
    Serial.print('\'');
    if(m2m.add(valueToSend) == false)
    {
      Serial.print(F(" failed"));
    }
  }
  else  //Send an array
  {
    uint8_t arraySize = random(2,9);
    char valuesToSend[arraySize];
    Serial.print(F("\r\n\tchar["));
    Serial.print(arraySize);
    Serial.print(F("]: "));
    for(uint8_t index = 0; index < arraySize; index++)
    {
      valuesToSend[index] = randomChar();
      Serial.print(valuesToSend[index]);
      Serial.print(' ');
    }
    if(m2m.add(valuesToSend, arraySize) == false)
    {
      Serial.print(F("failed"));
    }
  }
  fieldsToSend--;
}
char randomChar()
{
  return (char)random(32,127);
}
void addRandomStr()
{
  switch (random(0,3))
  {
    case 0:
      Serial.print(F("\r\n\tNull terminated char["));
      Serial.print(strlen(shortTestStr)+1);
      Serial.print(F("] array: '"));
      Serial.print(shortTestStr);
      Serial.print('\'');
      if(m2m.addStr(shortTestStr) == false)
      {
        Serial.print(F(" failed"));
      }
      fieldsToSend--;
    break;
    case 1:
      Serial.print(F("\r\n\tNull terminated char["));
      Serial.print(strlen(mediumTestStr)+1);
      Serial.print(F("] array: '"));
      Serial.print(mediumTestStr);
      Serial.print('\'');
      if(m2m.addStr(mediumTestStr) == false)
      {
        Serial.print(F(" failed"));
      }
      fieldsToSend--;
    break;
    case 2:
      Serial.print(F("\r\n\tNull terminated char["));
      Serial.print(strlen(utf8TestStr)+1);
      Serial.print(F("] array: '"));
      Serial.print(utf8TestStr);
      Serial.print('\'');
      if(m2m.addStr(utf8TestStr) == false)
      {
        Serial.print(F(" failed"));
      }
      fieldsToSend--;
    break;
  }
}
