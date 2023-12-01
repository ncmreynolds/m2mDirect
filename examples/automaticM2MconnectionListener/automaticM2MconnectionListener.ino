/*
 * This sketch connects two devices automatically the first time it runs, exchanging MAC address and encryption key
 * 
 * After this it will store and re-use the details from the first connection so should connect more quickly
 * 
 * This sketch will show the contents of any packets received on the Serial monitor
 * 
 * If you hold the optional pairing button (usually connected at BOOT/GPIO0 on many dev boards) for 5s it will re-pair again
 * 
 * This demonstrates the basic functions of the library, including (optional) callbacks and how to retrieve data
 * 
 * It can also be used as a 'sniffer' in your projects that you create with this library, showing you the data sent in detail
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
  Serial.printf(PSTR("\n\rReceived message with %u data fields, link quality: %02x"), m2m.dataAvailable(), m2m.linkQuality());
  while(m2m.dataAvailable() > 0) //dataAvailable is the number of fields of data in a message
  {
    switch (m2m.nextDataType())
    {
      case m2m.DATA_BOOL:
        retrieveBool();
      break;
      case m2m.DATA_BOOL_ARRAY:
        retrieveBoolArray();
      break;
      case m2m.DATA_UINT8_T:
        retrieveUint8_t();
      break;
      case m2m.DATA_UINT8_T_ARRAY:
        retrieveUint8_tArray();
      break;
      case m2m.DATA_UINT16_T:
        retrieveUint16_t();
      break;
      case m2m.DATA_UINT16_T_ARRAY:
        retrieveUint16_tArray();
      break;
      case m2m.DATA_UINT32_T:
        retrieveUint32_t();
      break;
      case m2m.DATA_UINT32_T_ARRAY:
        retrieveUint32_tArray();
      break;
      case m2m.DATA_UINT64_T:
        retrieveUint64_t();
      break;
      case m2m.DATA_UINT64_T_ARRAY:
        retrieveUint64_tArray();
      break;
      case m2m.DATA_INT8_T:
        retrieveInt8_t();
      break;
      case m2m.DATA_INT8_T_ARRAY:
        retrieveInt8_tArray();
      break;
      case m2m.DATA_INT16_T:
        retrieveInt16_t();
      break;
      case m2m.DATA_INT16_T_ARRAY:
        retrieveInt16_tArray();
      break;
      case m2m.DATA_INT32_T:
        retrieveInt32_t();
      break;
      case m2m.DATA_INT32_T_ARRAY:
        retrieveInt32_tArray();
      break;
      case m2m.DATA_INT64_T:
        retrieveInt64_t();
      break;
      case m2m.DATA_INT64_T_ARRAY:
        retrieveInt64_tArray();
      break;
      case m2m.DATA_FLOAT:
        retrieveFloat();
      break;
      case m2m.DATA_FLOAT_ARRAY:
        retrieveFloatArray();
      break;
      case m2m.DATA_DOUBLE:
        retrieveDouble();
      break;
      case m2m.DATA_DOUBLE_ARRAY:
        retrieveDoubleArray();
      break;
      case m2m.DATA_CHAR:
        retrieveChar();
      break;
      case m2m.DATA_CHAR_ARRAY:
        retrieveCharArray();
      break;
      case m2m.DATA_STR:
        retrieveStr();
      break;
      case m2m.DATA_CUSTOM:
        m2m.skipReceivedData(); //Skips the next field
        Serial.print(F("\n\r\tCustom: skipped"));
        m2m.clearReceivedMessage();  //Clear the received message to wait for the next message
      break;
      default:
        Serial.print(F("\n\r\tUnable to retrieve received data, unknown type in message"));
        m2m.clearReceivedMessage();  //Clear the received message to wait for the next message
      break;
    }
  }
}


void setup()
{
  Serial.begin(115200); //Start the serial interface for debug
  delay(500); //Give some time for the Serial Monitor to come online
  //m2m.debug(Serial);  //Tell the library to use Serial for debug output
  m2m.pairingButtonGpio(0); //Enable the pairing button on GPIO 0
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
}
void retrieveBool()
{
  bool receivedData = false;
  if(m2m.retrieve(&receivedData)) //You must pass by reference the variable to retrieve the data into
  {
    Serial.print(F("\n\r\tbool: "));
    if(receivedData)
    {
      Serial.print(F("true"));
    }
    else
    {
      Serial.print(F("false"));
    }
  }
  else
  {
    Serial.print(F("\n\r\tunable to retrieve received bool data"));
    m2m.clearReceivedMessage();  //Clear the received message to wait for the next message
  }
}
void retrieveBoolArray()
{
  uint8_t arrayLength = m2m.nextDataLength();
  bool receivedData[arrayLength];
  if(m2m.retrieve(receivedData, arrayLength)) //You must pass by reference the variable to retrieve the data into and for arrays supply the length
  {
    Serial.print(F("\n\r\tbool["));
    Serial.print(arrayLength);
    Serial.print(F("]: "));
    for(uint8_t index = 0; index < arrayLength; index++)
    {
      Serial.print(receivedData[index] == true ? "true" : "false");
      Serial.print(' ');
    }
  }
  else
  {
    Serial.print(F("\n\r\tunable to retrieve received bool array data"));
    m2m.clearReceivedMessage();  //Clear the received message to wait for the next message
  }
}
void retrieveUint8_t()
{
  uint8_t receivedData = 0;
  if(m2m.retrieve(&receivedData)) //You must pass by reference the variable to retrieve the data into
  {
    Serial.print(F("\n\r\tuint8_t: "));
    Serial.print(receivedData);
  }
  else
  {
    Serial.print(F("\n\r\tunable to retrieve received uint8_t data"));
    m2m.clearReceivedMessage();  //Clear the received message to wait for the next message
  }
}
void retrieveUint8_tArray()
{
  uint8_t arrayLength = m2m.nextDataLength();
  uint8_t receivedData[arrayLength];
  if(m2m.retrieve(receivedData, arrayLength)) //You must pass by reference the variable to retrieve the data into and for arrays supply the length
  {
    Serial.print(F("\n\r\tuint8_t["));
    Serial.print(arrayLength);
    Serial.print(F("]: "));
    for(uint8_t index = 0; index < arrayLength; index++)
    {
      Serial.print(receivedData[index]);
      Serial.print(' ');
    }
  }
  else
  {
    Serial.print(F("\n\r\tunable to retrieve received uint8_t array data"));
    m2m.clearReceivedMessage();  //Clear the received message to wait for the next message
  }
}
void retrieveUint16_t()
{
  uint16_t receivedData = 0;
  if(m2m.retrieve(&receivedData)) //You must pass by reference the variable to retrieve the data into
  {
    Serial.print(F("\n\r\tuint16_t: "));
    Serial.print(receivedData);
  }
  else
  {
    Serial.print(F("\n\r\tunable to retrieve received uint16_t data"));
    m2m.clearReceivedMessage();  //Clear the received message to wait for the next message
  }
}
void retrieveUint16_tArray()
{
  uint8_t arrayLength = m2m.nextDataLength();
  uint16_t receivedData[arrayLength];
  if(m2m.retrieve(receivedData, arrayLength)) //You must pass by reference the variable to retrieve the data into and for arrays supply the length
  {
    Serial.print(F("\n\r\tuint16_t["));
    Serial.print(arrayLength);
    Serial.print(F("]: "));
    for(uint8_t index = 0; index < arrayLength; index++)
    {
      Serial.print(receivedData[index]);
      Serial.print(' ');
    }
  }
  else
  {
    Serial.print(F("\n\r\tunable to retrieve received uint16_t array data"));
    m2m.clearReceivedMessage();  //Clear the received message to wait for the next message
  }
}
void retrieveUint32_t()
{
  uint32_t receivedData = 0;
  if(m2m.retrieve(&receivedData)) //You must pass by reference the variable to retrieve the data into
  {
    Serial.print(F("\n\r\tuint32_t: "));
    Serial.print(receivedData);
  }
  else
  {
    Serial.print(F("\n\r\tunable to retrieve received uint32_t data"));
    m2m.clearReceivedMessage();  //Clear the received message to wait for the next message
  }
}
void retrieveUint32_tArray()
{
  uint8_t arrayLength = m2m.nextDataLength();
  uint32_t receivedData[arrayLength];
  if(m2m.retrieve(receivedData, arrayLength)) //You must pass by reference the variable to retrieve the data into and for arrays supply the length
  {
    Serial.print(F("\n\r\tuint32_t["));
    Serial.print(arrayLength);
    Serial.print(F("]: "));
    for(uint8_t index = 0; index < arrayLength; index++)
    {
      Serial.print(receivedData[index]);
      Serial.print(' ');
    }
  }
  else
  {
    Serial.print(F("\n\r\tunable to retrieve received uint32_t array data"));
    m2m.clearReceivedMessage();  //Clear the received message to wait for the next message
  }
}
void retrieveUint64_t()
{
  uint64_t receivedData = 0;
  if(m2m.retrieve(&receivedData)) //You must pass by reference the variable to retrieve the data into
  {
    Serial.print(F("\n\r\tuint64_t: "));
    Serial.print(receivedData);
  }
  else
  {
    Serial.print(F("\n\r\tunable to retrieve received uint64_t data"));
    m2m.clearReceivedMessage();  //Clear the received message to wait for the next message
  }
}
void retrieveUint64_tArray()
{
  uint8_t arrayLength = m2m.nextDataLength();
  uint64_t receivedData[arrayLength];
  if(m2m.retrieve(receivedData, arrayLength)) //You must pass by reference the variable to retrieve the data into and for arrays supply the length
  {
    Serial.print(F("\n\r\tuint64_t["));
    Serial.print(arrayLength);
    Serial.print(F("]: "));
    for(uint8_t index = 0; index < arrayLength; index++)
    {
      Serial.print(receivedData[index]);
      Serial.print(' ');
    }
  }
  else
  {
    Serial.print(F("\n\r\tunable to retrieve received uint64_t array data"));
    m2m.clearReceivedMessage();  //Clear the received message to wait for the next message
  }
}
void retrieveInt8_t()
{
  int8_t receivedData = 0;
  if(m2m.retrieve(&receivedData)) //You must pass by reference the variable to retrieve the data into
  {
    Serial.print(F("\n\r\tint8_t: "));
    Serial.print(receivedData);
  }
  else
  {
    Serial.print(F("\n\r\tunable to retrieve received int8_t data"));
    m2m.clearReceivedMessage();  //Clear the received message to wait for the next message
  }
}
void retrieveInt8_tArray()
{
  uint8_t arrayLength = m2m.nextDataLength();
  int8_t receivedData[arrayLength];
  if(m2m.retrieve(receivedData, arrayLength)) //You must pass by reference the variable to retrieve the data into and for arrays supply the length
  {
    Serial.print(F("\n\r\tint8_t["));
    Serial.print(arrayLength);
    Serial.print(F("]: "));
    for(uint8_t index = 0; index < arrayLength; index++)
    {
      Serial.print(receivedData[index]);
      Serial.print(' ');
    }
  }
  else
  {
    Serial.print(F("\n\r\tunable to retrieve received int8_t array data"));
    m2m.clearReceivedMessage();  //Clear the received message to wait for the next message
  }
}
void retrieveInt16_t()
{
  int16_t receivedData = 0;
  if(m2m.retrieve(&receivedData)) //You must pass by reference the variable to retrieve the data into
  {
    Serial.print(F("\n\r\tint16_t: "));
    Serial.print(receivedData);
  }
  else
  {
    Serial.print(F("\n\r\tunable to retrieve received int16_t data"));
    m2m.clearReceivedMessage();  //Clear the received message to wait for the next message
  }
}
void retrieveInt16_tArray()
{
  uint8_t arrayLength = m2m.nextDataLength();
  int16_t receivedData[arrayLength];
  if(m2m.retrieve(receivedData, arrayLength)) //You must pass by reference the variable to retrieve the data into and for arrays supply the length
  {
    Serial.print(F("\n\r\tint16_t["));
    Serial.print(arrayLength);
    Serial.print(F("]: "));
    for(uint8_t index = 0; index < arrayLength; index++)
    {
      Serial.print(receivedData[index]);
      Serial.print(' ');
    }
  }
  else
  {
    Serial.print(F("\n\r\tunable to retrieve received int16_t array data"));
    m2m.clearReceivedMessage();  //Clear the received message to wait for the next message
  }
}
void retrieveInt32_t()
{
  int32_t receivedData = 0;
  if(m2m.retrieve(&receivedData)) //You must pass by reference the variable to retrieve the data into
  {
    Serial.print(F("\n\r\tint32_t: "));
    Serial.print(receivedData);
  }
  else
  {
    Serial.print(F("\n\r\tunable to retrieve received int32_t data"));
    m2m.clearReceivedMessage();  //Clear the received message to wait for the next message
  }
}
void retrieveInt32_tArray()
{
  uint8_t arrayLength = m2m.nextDataLength();
  int32_t receivedData[arrayLength];
  if(m2m.retrieve(receivedData, arrayLength)) //You must pass by reference the variable to retrieve the data into and for arrays supply the length
  {
    Serial.print(F("\n\r\tint32_t["));
    Serial.print(arrayLength);
    Serial.print(F("]: "));
    for(uint8_t index = 0; index < arrayLength; index++)
    {
      Serial.print(receivedData[index]);
      Serial.print(' ');
    }
  }
  else
  {
    Serial.print(F("\n\r\tunable to retrieve received int32_t array data"));
    m2m.clearReceivedMessage();  //Clear the received message to wait for the next message
  }
}
void retrieveInt64_t()
{
  int64_t receivedData = 0;
  if(m2m.retrieve(&receivedData)) //You must pass by reference the variable to retrieve the data into
  {
    Serial.print(F("\n\r\tint64_t: "));
    Serial.print(receivedData);
  }
  else
  {
    Serial.print(F("\n\r\tunable to retrieve received int64_t data"));
    m2m.clearReceivedMessage();  //Clear the received message to wait for the next message
  }
}
void retrieveInt64_tArray()
{
  uint8_t arrayLength = m2m.nextDataLength();
  int64_t receivedData[arrayLength];
  if(m2m.retrieve(receivedData, arrayLength)) //You must pass by reference the variable to retrieve the data into and for arrays supply the length
  {
    Serial.print(F("\n\r\tint64_t["));
    Serial.print(arrayLength);
    Serial.print(F("]: "));
    for(uint8_t index = 0; index < arrayLength; index++)
    {
      Serial.print(receivedData[index]);
      Serial.print(' ');
    }
  }
  else
  {
    Serial.print(F("\n\r\tunable to retrieve received int64_t array data"));
    m2m.clearReceivedMessage();  //Clear the received message to wait for the next message
  }
}
void retrieveFloat()
{
  float receivedData = 0.0;
  if(m2m.retrieve(&receivedData)) //You must pass by reference the variable to retrieve the data into
  {
    Serial.print(F("\n\r\tfloat: "));
    Serial.print(receivedData);
  }
  else
  {
    Serial.print(F("\n\r\tunable to retrieve received float data"));
    m2m.clearReceivedMessage();  //Clear the received message to wait for the next message
  }
}
void retrieveFloatArray()
{
  uint8_t arrayLength = m2m.nextDataLength();
  float receivedData[arrayLength];
  if(m2m.retrieve(receivedData, arrayLength)) //You must pass by reference the variable to retrieve the data into and for arrays supply the length
  {
    Serial.print(F("\n\r\tfloat["));
    Serial.print(arrayLength);
    Serial.print(F("]: "));
    for(uint8_t index = 0; index < arrayLength; index++)
    {
      Serial.print(receivedData[index]);
      Serial.print(' ');
    }
  }
  else
  {
    Serial.print(F("\n\r\tunable to retrieve received float array data"));
    m2m.clearReceivedMessage();  //Clear the received message to wait for the next message
  }
}
void retrieveDouble()
{
  double receivedData = 0;
  if(m2m.retrieve(&receivedData)) //You must pass by reference the variable to retrieve the data into
  {
    Serial.print(F("\n\r\tdouble: "));
    Serial.print(receivedData);
  }
  else
  {
    Serial.print(F("\n\r\tunable to retrieve received double data"));
    m2m.clearReceivedMessage();  //Clear the received message to wait for the next message
  }
}
void retrieveDoubleArray()
{
  uint8_t arrayLength = m2m.nextDataLength();
  double receivedData[arrayLength];
  if(m2m.retrieve(receivedData, arrayLength)) //You must pass by reference the variable to retrieve the data into and for arrays supply the length
  {
    Serial.print(F("\n\r\tdouble["));
    Serial.print(arrayLength);
    Serial.print(F("]: "));
    for(uint8_t index = 0; index < arrayLength; index++)
    {
      Serial.print(receivedData[index]);
      Serial.print(' ');
    }
  }
  else
  {
    Serial.print(F("\n\r\tunable to retrieve received double array data"));
    m2m.clearReceivedMessage();  //Clear the received message to wait for the next message
  }
}
void retrieveChar()
{
  char receivedData = 0;
  if(m2m.retrieve(&receivedData)) //You must pass by reference the variable to retrieve the data into
  {
    Serial.print(F("\n\r\tchar: '"));
    Serial.print(receivedData);
    Serial.print('\'');
  }
  else
  {
    Serial.print(F("\n\r\tunable to retrieve received char data"));
    m2m.clearReceivedMessage();  //Clear the received message to wait for the next message
  }
}
void retrieveCharArray()
{
  uint8_t arrayLength = m2m.nextDataLength();
  char receivedData[arrayLength];
  if(m2m.retrieve(receivedData, arrayLength)) //You must pass by reference the variable to retrieve the data into and for arrays supply the length
  {
    Serial.print(F("\n\r\tchar["));
    Serial.print(arrayLength);
    Serial.print(F("]: "));
    for(uint8_t index = 0; index < arrayLength; index++)
    {
      Serial.print('\'');
      Serial.print(receivedData[index]);
      Serial.print(F("' "));
    }
  }
  else
  {
    Serial.print(F("\n\r\tunable to retrieve received char array data"));
    m2m.clearReceivedMessage();  //Clear the received message to wait for the next message
  }
}
void retrieveStr()
{
  uint8_t length = m2m.nextDataLength();
  char receivedData[length + 1];
  if(m2m.retrieveStr(receivedData)) //You must pass by reference the variable to retrieve the data into, unless it is an 'array' like a C string
  {
    Serial.print(F("\n\r\tNull terminated char["));
    Serial.print(length + 1);
    Serial.print(F("] array: '"));
    Serial.print(receivedData);
    Serial.print('\'');
  }
  else
  {
    Serial.print(F("\n\r\tunable to retrieve received char array data"));
    m2m.clearReceivedMessage();  //Clear the received message to wait for the next message
  }
}
