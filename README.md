# m2mDirect
## Introduction

An Arduino library for connecting and maintaining a point-to-point connection between two (and only two) Espressif ESP~~8266~~/~~8285~~/32 microcontrollers with ESP-NOW. This library uses the inbuilt encryption features of ESP-NOW to provide a minimal level of privacy, attempts to confirm delivery of packets and monitor the quality of the connection.

It should not be considered 'secure' or 'reliable' for serious use but once paired should be more robust than the typical 'fire and forget' approaches to using ESP-NOW.

The pairing process shares randomly generated encryption keys in an insecure manner but this only happens during pairing. Once paired the two devices store this in flash/EEPROM and do not share them again unless re-flashed or paired again.

## What to use this library for

This library is for when you want a low-ish latency two way communication method for sending small amounts of data between two (and only two) microcontrollers and there is no 'infrastructure' such as Wi-Fi access points, MQTT servers etc. to rely on.

It was written to provide similar functionality to hobby remote control devices with return telemetry in a robot project and works well for this.

## What not to use this library for

This library is not suited to to transferring larger amounts of data such as files, pictures, etc. or anything needing strong encryption or absolutely guaranteed delivery of packets.

While the link failure detection is reasonably robust you should not rely on it for safety critical projects eg. the control of a heavy/fast/dangerous device.

## Device names

Each device can optionally have a name, which is a pure convenience thing to use in user interfaces or logs.

```
m2mDirect.localName("My device");  //Set the name of the device
```

This sets the name of the local device.

```
if(m2mDirect.remoteNameSet() == true)
{
	Serial.print(F("\n\rPaired with device: "));
	Serial.print(m2mDirect.remoteName());
}
```

This lets the application access the name of the remote device, if set and paired.

## Channel usage

~~This library will choose a 2.4Ghz channel on pairing from channels 1, 6 & 11 based on the least number of BSSIDs visible on that channel during pairing and re-use that choice every time. It can optionally be configured to re-scan channels on start-up, but this will slow down the connection process.~~

~~You can optionally hard configure a fixed channel in the range 1-14, but be aware channels 12-13 are not legal in most countries (legal at reduced power in the US) and channel 14 is only legal in Japan.~~

Currently the channel is hard set when the library is initialised, but the aspiration is to negotiate a channel with the fewest other SSIDs with the lowest dBm. Some code for this is already in place but needs work.

```
m2mDirect.begin(1);  //Start the M2M connection on channel 1
```

## Housekeeping

The library relies on regular housekeeping to send keepalive packets. This must be called regularly in the main loop, if it is frequently delayed then the link will be considered disconnected. This *may* move to an RTOS task on ESP32 at some point in the future.

```
void loop()
{
  m2mDirect.housekeeping(); //Maintain the M2M connection
  //Do other things
}
```

## Pairing

When set to pair, both devices will share information about themselves on Wi-Fi channel 1, in the clear unencrypted. You should not re-pair unless absolutely necessary as it exposes the encryption keys.

```
m2mDirect.pairingButtonGpio(0); //Enable the pairing button on GPIO 0 which is often available on ESP board as the 'program' button
```

This configures a button on the specified GPIO pin that will cause it to re-pair when you hold it down for five seconds.

```
m2mDirect.resetPairing();	//Reset the pairing information and start trying to pair
```

This immediately causes the device to start pairing.

```
m2mDirect.indicatorGpio(LED_BUILTIN);  //Enable the indicator LED on LED_BUILTIN
```

This a configures an indicator on the specified GPIO pin (typically for an LED) that shows the pairing state and link quality. If configured, the link LED behaves in the following way.

- Solid - paired and connected
- Slow flashing - pairing
- Fast flashing - attempting to connect

## Transmission power management

This library will moderate transmission power downwards when transmitted packets are 100% successful and moderate it upwards when they are not. This is to attempt to be a 'good neighbour' in the often crowded 2.4Ghz space.

Optionally, transmit power can be left fixed.

```
m2mDirect.setAutomaticTxPower(true);	//Enable automatic transmit power (default)
m2mDirect.setAutomaticTxPower(false);	//Disable automatic transmit power
```

## Reliability

By default this library uses the receive callback feature in ESP-NOW to confirm the other end of the link has received the sent data. This is not 100% reliable but is a fair indication of delivery. As a result the default sending method is synchronous and does not return instantly. ~~The timeout for this can be adjusted.~~

Optionally, data can be sent asynchronously and the application check for delivery later. In neither case is data retained to be retried after this initial failure, it is for the application to handle this if necessary.

The library also sends periodic keepalives to provide a measure of link reliability. These keepalives mean the application can check the state of the link before trying to send data etc.

```
if(m2mDirect.connected())
{
	//Do stuff
}
```

All keepalives have a rotating sequence number, recent missed packets will be detected and reduce the measure of link quality, even if at time of sending they were apparently delivered.  Link reliability is measured as a 32-bit rolling bitfield with the most significant bit being the most recent so 'higher is better'.

```
uint32_t linkQuality = m2mDirect.linkQuality();
```

## Payload

This library uses six bytes in each packet for signalling, reducing the effective packet size for user data to 242 bytes. If more payload than this is needed, it is for the application to fragment the data. Future versions of the library may buffer application payload, removing this work from the user application.

### Adding single values to the payload

Values or arrays of values are 'added' to a message then sent.

    if(m2mDirect.add(valueToSend) == true)
    {
    	Serial.print(F("Added"));
    }
    else
    {
    	Serial.print(F("Not added"));
    }


This sends a single value, with the library automatically 'serialising' the data into the message storing its type. Null-terminated C strings are a special case. 

    if(m2mDirect.addStr(stringToSend) == true)
    {
    	Serial.print(F("Added"));
    }
    else
    {
    	Serial.print(F("Not added"));
    }

### Adding arrays to the payload

To send an array of values they are similarly 'added', but the application must supply a length for the number of entries in the array. There is no support for an array of C strings. Arrays of characters that are a not null-terminated are considered an array and may contain zeroes.

    if(m2mDirect.add(valuesToSend, arraySize) == false)
    {
    	Serial.print(F("Added"));
    }
    else
    {
    	Serial.print(F("Not added"));
    }

## Sending the message

Sending is quite simple. Note it is synchronous, the sketch will pause briefly to confirm receipt, which is how there can be meaningful feedback to the application.

    if(m2mDirect.sendMessage())
    {
    	Serial.print(F("Sent"));
    }
    else
    {
    	Serial.print(F("Not sent"));
    }

Optionally the message can be sent immediately without waiting.

```
m2mDirect.sendMessage(false);
```

## Receiving messages

The expected model for the application is an event driven one with callbacks. See the examples for more detail.

```
void onMessageReceived()
{
	Serial.printf(PSTR("\n\rReceived message, link quality: %02x"),m2mDirect.linkQuality());
	//Do stuff
}

void setup()
{
  Serial.begin(115200); //Start the serial interface for debug
  //Other setup
  m2mDirect.setMessageReceivedCallback(onMessageReceived);  //Set the 'message received' callback created above
  m2mDirect.begin(1);  //Start the M2M connection on channel 1
}
```

The application should not do massive amounts of blocking work in the callback, ideally load the received data into variables and work on it in the main loop. If the application does not read all the data from the message payload, any further incoming messages will be discarded.

### Determining types of the payload

The payload stores the types of each piece of data in the payload, which are uint8_t constants.

```
uint8_t type = m2mDirect.nextDataType();
//Possible data types
m2mDirect.DATA_UNAVAILABLE		//Used to denote no more data left
m2mDirect.DATA_BOOL 			//Used to denote boolean, it also implies the boolean is false
m2mDirect.DATA_UINT8_T			//Used to denote an uint8_t in user data
m2mDirect.DATA_UINT16_T			//Used to denote an uint16_t in user data
m2mDirect.DATA_UINT32_T			//Used to denote an uint32_t in user data
m2mDirect.DATA_UINT64_T			//Used to denote an uint64_t in user data
m2mDirect.DATA_INT8_T			//Used to denote an int8_t in user data
m2mDirect.DATA_INT16_T			//Used to denote an int16_t in user data
m2mDirect.DATA_INT32_T			//Used to denote an int32_t in user data
m2mDirect.DATA_INT64_T			//Used to denote an int64_t in user data
m2mDirect.DATA_FLOAT			//Used to denote a float (32-bit) in user data
m2mDirect.DATA_DOUBLE			//Used to denote a double float (64-bit) in user data
m2mDirect.DATA_CHAR				//Used to denote a char in user data
m2mDirect.DATA_STR				//Used to denote a null terminated C string in user data
m2mDirect.DATA_KEY				//Used to denote a key, which is a null terminated C string in user data
m2mDirect.DATA_CUSTOM			//Used to denote a custom type in user data
m2mDirect.DATA_BOOL_ARRAY		//Used to denote boolean array in user data
m2mDirect.DATA_UINT8_T_ARRAY	//Used to denote an uint8_t array in user data
m2mDirect.DATA_UINT16_T_ARRAY	//Used to denote an uint16_t array in user data
m2mDirect.DATA_UINT32_T_ARRAY	//Used to denote an uint32_t array in user data
m2mDirect.DATA_UINT64_T_ARRAY	//Used to denote an uint64_t array in user data
m2mDirect.DATA_INT8_T_ARRAY		//Used to denote an int8_t array in user data
m2mDirect.DATA_INT16_T_ARRAY	//Used to denote an int16_t array in user data
m2mDirect.DATA_INT32_T_ARRAY	//Used to denote an int32_t array in user data
m2mDirect.DATA_INT64_T_ARRAY	//Used to denote an int64_t array in user data
m2mDirect.DATA_FLOAT_ARRAY		//Used to denote a float (32-bit) array in user data
m2mDirect.DATA_DOUBLE_ARRAY		//Used to denote a double float (64-bit) array in user data
m2mDirect.DATA_CHAR_ARRAY		//Used to denote a char array in user data (not a null terminated C string!)
m2mDirect.DATA_CUSTOM_ARRAY		//Used to denote a custom type array in user data

```

### Retrieving single values from the payload

Pass a variable of the appropriate type by reference and it will be set the value in the payload. If the variable does not match the type in the payload or the application has already retrieved everything, nothing will be retrieved.

```
if(m2mDirect.retrieve(&receivedData)) //Pass by reference the variable to retrieve the data into
{
	Serial.print(F("Retrieved"))
}
else
{
	Serial.print(F("Not retrieved"))
}
```

A null-terminated C string is again a special case. The application must check and use the length for the char array the application supplies. The length reported excludes the character for null termination, it must take this into account. If retrieved the string will always be null terminated.

```
uint8_t length = m2mDirect.nextDataLength();
char receivedData[length + 1];
if(m2mDirect.retrieveStr(receivedData))
{
	Serial.print(F("Retrieved"))
}
else
{
	Serial.print(F("Not retrieved"))
}
```

### Retrieving arrays from the payload

Like with a null-terminated C string the application must check and use the length for the char array it supplies. Both type and length must match for it to be retrieved.

```
uint8_t arrayLength = m2mDirect.nextDataLength();
float receivedData[arrayLength];
if(m2mDirect.retrieve(receivedData, arrayLength))
{
	Serial.print(F("Retrieved"))
}
else
{
	Serial.print(F("Not retrieved"))
}
```

### Clearing remaining data

On occasion it may make sense to discard some or all of the data in a message.

```
m2mDirect.clearReceivedMessage();  //Clear the received message to wait for the next message
```

## Other callbacks

In keeping with the event driven model, there are other callbacks the application can set to keep it apprised of the state of the link.

```
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
    if(m2mDirect.remoteNameSet() == true)
    {
        Serial.print(F("\n\rPaired with device: "));
        Serial.print(m2mDirect.remoteName());
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
    if(m2mDirect.remoteNameSet() == true)
    {
        Serial.print(F("\n\rConnected to device: "));
        Serial.print(m2mDirect.remoteName());
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
    if(m2mDirect.remoteNameSet() == true)
    {
        Serial.print(F("\n\rDisconnected from device: "));
        Serial.print(m2mDirect.remoteName());
    }
    else
    {
	    Serial.print(F("\n\rDisconnected"));
    }
}
void setup()
{
  Serial.begin(115200); //Start the serial interface for debug
  delay(500); //Give some time for the Serial Monitor to come online
  //m2mDirect.debug(Serial);  //Tell the library to use Serial for debug output
  m2mDirect.pairingButtonGpio(0); //Enable the pairing button on GPIO 0 which is often available on ESP board as the 'program' button
  #ifdef LED_BUILTIN
    m2mDirect.indicatorGpio(LED_BUILTIN);  //Enable the indicator LED
  #endif
  m2mDirect.localName(name);  //Set the name of the device
  m2mDirect.setPairingCallback(onPairStart);  //Set the 'pairing' callback created above
  m2mDirect.setPairedCallback(onPaired);  //Set the 'paired' callback created above
  m2mDirect.setConnectedCallback(onConnected);  //Set the 'connected' callback created above
  m2mDirect.setDisconnectedCallback(onDisconnected);  //Set the 'disconnected' callback created above
  m2mDirect.setMessageReceivedCallback(onMessageReceived);  //Set the 'message received' callback created above
  m2mDirect.setAutomaticTxPower(true); //Enable automatic adjustment of transmit power (default, use false to disable this)
  m2mDirect.begin(1);  //Start the M2M connection on channel 1
}

```



## Known Issues/Omissions

- Channel selection is non-functional, forcing to channel 1 for now
- Skipping a data field is broken and will probably cause an exception or misreading of later fields
- No support for ESP8266 yet. This is just due to minor API differences that need coding
- There is a 'bounce' on initial connection where devices go connected->disconnected->connected quickly and reliable. This needs investigating.
- There is an aspiration to support key/value pairs for those who like that model. This will mean much less can be squeezed into each packet though.
