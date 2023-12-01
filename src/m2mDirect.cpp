/*
 *	An Arduino library for connecting and maintaining a point-to-point connection between two Espressif ESP8266/8285/32 microcontrollers with ESP-NOW.
 *
 *	This library uses the inbuilt encryption features of ESP-NOW to provide a minimal level of privacy and attempts to ensure delivery of packets and monitor the quality of the connection.
 *
 *	https://github.com/ncmreynolds/m2mDirect
 *
 *	Released under LGPL-2.1 see https://github.com/ncmreynolds/m2mDirect/LICENSE for full license
 *
 */
#ifndef m2mDirect_cpp
#define m2mDirect_cpp
#ifndef m2mDirect_h
#include "m2mDirect.h"
#endif

m2mDirectClass::m2mDirectClass()	//Constructor function
{
}

m2mDirectClass::~m2mDirectClass()	//Destructor function
{
}
/*
 *
 *	Initialise the connection, running through various startup steps
 *
 */
/*
 *
 *	Sets the local device name
 *
 */
 
void ICACHE_FLASH_ATTR m2mDirectClass::localName(String nameToSet) {
	localDeviceName = new char[nameToSet.length() + 1];
	if(localDeviceName != nullptr)
	{
		nameToSet.toCharArray(localDeviceName, nameToSet.length() + 1);
		if(debug_uart_ != nullptr)
		{
			debug_uart_->printf_P(PSTR("\n\rDevice name set to %s"),localDeviceName);
		}
	}
	else
	{
		if(debug_uart_ != nullptr)
		{
			debug_uart_->print(F("\n\rFailed to set device name"));
		}
	}
}
/*
 *
 *	Returns a pointer to the local device name
 *
 */

char* ICACHE_FLASH_ATTR m2mDirectClass::localName()
{
	return localDeviceName;
}
/*
 *
 *	Returns true if the remote device has a name
 *
 */
bool ICACHE_FLASH_ATTR m2mDirectClass::remoteNameSet()
{
	if(remoteDeviceName != nullptr)
	{
		return true;
	}
	return false;
}
/*
 *
 *	Returns a pointer to the remote device name
 *
 */
char* ICACHE_FLASH_ATTR m2mDirectClass::remoteName()
{
	return remoteDeviceName;
}
/*
 *
 *	Sets up the data structures, callbacks and so on then start the process of sending packets
 *
 */
#if defined(ESP8266) || defined(ESP32)
void ICACHE_FLASH_ATTR m2mDirectClass::begin(uint8_t communicationChannel, uint8_t pairingChannel)	{
#else
void m2mDirectClass::begin(uint8_t communicationChannel, uint8_t pairingChannel)	{
#endif
	if(debug_uart_ != nullptr)
	{
		debug_uart_->print(F("\n\rm2mDirect starting"));
	}
	if(_pairingButtonGpio != 255)
	{
		if(debug_uart_ != nullptr)
		{
			debug_uart_->print(F("\n\rConfiguring pairing button on GPIO pin: "));
			debug_uart_->print(_pairingButtonGpio);
			if(_pairingButtonGpioNc == true)
			{
				debug_uart_->print(F(" (normally closed)"));
			}
			else
			{
				debug_uart_->print(F(" (normally open)"));
			}
		}
		pinMode(_pairingButtonGpio, INPUT_PULLUP);
	}
	if(_indicatorLedGpio != 255)
	{
		if(debug_uart_ != nullptr)
		{
			debug_uart_->print(F("\n\rConfiguring indicator LED on GPIO pin: "));
			debug_uart_->print(_indicatorLedGpio);
			if(_indicatorLedGpioInverted == true)
			{
				debug_uart_->print(F(" (inverted)"));
			}
		}
		pinMode(_indicatorLedGpio, OUTPUT);
		_indicatorOff();
		_indicatorTimer = millis();
	}
	_communicationChannel = communicationChannel;
	_pairingChannel = pairingChannel;
	#if defined(ESP8266)
	EEPROM.begin(EEPROM_DATA_SIZE);	//Reads/writes the saved pairing from EEPROM
	#endif
	if(_readPairingInfo() == true)
	{
		_pairingInfoRead = true;
	}
	#if defined(ESP8266)
	//houseKeepingticker.attach_ms(100,[](){m2m.housekeeping();});
	#endif
}
/*
 *
 *	Configure a pin for the pairing GPIO
 *
 */
#if defined(ESP8266) || defined(ESP32)
void ICACHE_FLASH_ATTR m2mDirectClass::pairingButtonGpio(uint8_t pin, bool inverted)
#else
void m2mDirectClass::pairingButtonGpio(uint8_t pin, inverted)
#endif
{
	_pairingButtonGpio = pin;
	_pairingButtonGpioNc = inverted;
}
/*
 *
 *	Configure a pin for the indicator GPIO
 *
 */
#if defined(ESP8266) || defined(ESP32)
void ICACHE_FLASH_ATTR m2mDirectClass::indicatorGpio(uint8_t pin, bool inverted)
#else
void m2mDirectClass::indicatorGpio(uint8_t pin, inverted)
#endif
{
	_indicatorLedGpio = pin;
	_indicatorLedGpioInverted = inverted;
}
/*
 *
 *	Enable debugging on a specific stream
 *
 */
#if defined(ESP8266) || defined(ESP32)
void ICACHE_FLASH_ATTR m2mDirectClass::debug(Stream &terminalStream)
#else
void m2mDirectClass::debug(Stream &terminalStream)
#endif
{
	debug_uart_ = &terminalStream;		//Set the stream used for the terminal
	#if defined(ESP8266)
	if(&terminalStream == &Serial)
	{
		  debug_uart_->write(17);			//Send an XON to stop the hung terminal after reset on ESP8266
	}
	#endif
}
/*
 *
 *	This method needs running regularly to send keepalive and process incoming messages
 *
 */
#if defined(ESP8266) || defined(ESP32)
void ICACHE_FLASH_ATTR m2mDirectClass::housekeeping()
#else
void m2mDirectClass::housekeeping()
#endif
{
	if(_dataReceived == true)	//The application has data waiting
	{
		_dataReceived = false;
		if(messageReceivedCallback != nullptr) //Check this callback exists
		{
			messageReceivedCallback();
		}
		else
		{
			clearReceivedMessage();	//Mark it as 'read' so it doesn't clog the buffer
		}
	}
	if(state == m2mDirectState::uninitialised)	//Try to initialise if it failed on startup
	{
		if(millis() - _localActivityTimer > _pairingInterval)
		{
			_advanceTimers();	//Advance the timers for keepalives
			if(_initialiseWiFi() == true)
			{
				if(_initialiseEspNow(_pairingChannel) == true)
				{
					if(_pairingInfoRead == false)
					{
						state = m2mDirectState::initialised;
						_indicatorTimerInterval =  M2M_DIRECT_INDICATOR_LED_INITIALISED_INTERVAL;
						if(debug_uart_ != nullptr)
						{
							_debugState();
						}
					}
					else
					{
						state = m2mDirectState::connecting;
						_indicatorTimerInterval = M2M_DIRECT_INDICATOR_LED_CONNECTING_INTERVAL;
						if(debug_uart_ != nullptr)
						{
							_debugState();
						}
					}
					if(esp_wifi_get_max_tx_power(&_currentTxPower) == ESP_OK)
					{
						if(debug_uart_ != nullptr)
						{
							debug_uart_->printf_P(PSTR("\r\nStarting Tx power: %.2fdBm"), (float)_currentTxPower * 0.25);
						}
					}
				}
			}
		}
	}
	else if(state == m2mDirectState::initialised)	//Start the pairing process off by creating a new pairing message
	{
		_advanceTimers();	//Advance the timers for keepalives
		_sendQuality = _startingSendquality;	//Reset to defaults
		_echoQuality = _startingEchoQuality;	//Reset to defaults
		_keepaliveInterval = _startingKeepaliveInterval;	//Reset to defaults
		if(_pairingInfoRead == true)
		{
			if(m2m.pairedCallback != nullptr)
			{
				m2m.pairedCallback();
			}
			state = m2mDirectState::connecting;
			_indicatorTimerInterval = M2M_DIRECT_INDICATOR_LED_CONNECTING_INTERVAL;
			if(debug_uart_ != nullptr)
			{
				_debugState();
			}
		}
		else
		{
			if(_encyptionEnabled == true)
			{
				_chooseEncryptionKeys();
			}
			else
			{
				_clearEncryptionKeys();
			}
			_createPairingMessage();
			state = m2mDirectState::pairing;
			if(m2m.pairingCallback != nullptr)
			{
				m2m.pairingCallback();
			}
			_indicatorTimerInterval = M2M_DIRECT_INDICATOR_LED_PAIRING_INTERVAL;
			if(debug_uart_ != nullptr)
			{
				_debugState();
			}
		}
	}
	else if(state == m2mDirectState::pairing)
	{
		//Send a broadcast pairing message at intervals
		if(millis() - _localActivityTimer > _pairingInterval)
		{
			_sendBroadcastPacket(_protocolPacketBuffer, _protocolPacketBufferPosition);
			_advanceTimers();	//Advance the timers for keepalives
		}
	}
	else if(state == m2mDirectState::paired)
	{
		//Having received a pairing message, send a pairing ACK at intervals
		if(millis() - _localActivityTimer > _pairingInterval)
		{
			_sendBroadcastPacket(_protocolPacketBuffer, _protocolPacketBufferPosition);
			_advanceTimers();	//Advance the timers for keepalives
		}
	}
	else if(state == m2mDirectState::connecting)
	{
		//Boths ends have keys, start sending keepalives
		if(millis() - _localActivityTimer > _keepaliveInterval)
		{
			_createKeepaliveMessage();
			_sendUnicastPacket(_protocolPacketBuffer, _protocolPacketBufferPosition);	//Send quality and keepalive interval is automatically decremented in _sendUnicastPacket if it fails
			_advanceTimers();	//Advance the timers for keepalives
			//Check send quality
			if(linkQuality() > 0xFF000000)	//Assess AND of send and echo quality
			{
				if(_pairingInfoRead == false && _pairingInfoWritten == false) //Write the pairing info
				{
					if(_writePairingInfo())
					{
						_pairingInfoWritten = true;
					}
				}
				state = m2mDirectState::connected;
				if(_indicatorLedGpio != 255) //Switch on the indicator
				{
					_indicatorTimerInterval = M2M_DIRECT_INDICATOR_LED_CONNECTED_INTERVAL;
					_indicatorOn();
				}
				if(debug_uart_ != nullptr)
				{
					_debugState();
				}
				if(connectedCallback != nullptr)
				{
					connectedCallback();
				}
			}
		}
	}
	else if(state == m2mDirectState::connected)
	{
		//Connected state, monitor keepalives
		if(millis() - _localActivityTimer > _keepaliveInterval)
		{
			_createKeepaliveMessage();
			if(_automaticTxPower == true)
			{
				if(_sendQuality == 0xffffffff)
				{
					if(_currentTxPower == _minTxPower && _minTxPower > 9 && millis() - _lastTxPowerChange < _keepaliveInterval * 100) //Try reducing the minimum power after 100 good keepalives at the current minimum
					{
						_minTxPower--;
					}
					_reduceTxPower();
				}
				else
				{
					if(_lastTxPowerChangeDownwards == true && millis() - _lastTxPowerChange < _keepaliveInterval * 5) //A recent power reduction _probably_ caused packet loss
					{
						_minTxPower++;
					}
					_increaseTxPower();
				}
			}
			_sendUnicastPacket(_protocolPacketBuffer, _protocolPacketBufferPosition);	//Send quality and keepalive interval is automatically decremented in _sendUnicastPacket if it fails
			_advanceTimers();	//Advance the timers for keepalives
			//Check send quality
			if(_countBits(linkQuality()) < M2M_DIRECT_LINK_QUALITY_LOWER_THRESHOLD)	//Assess AND of send and echo quality
			{
				state = m2mDirectState::disconnected;
				_indicatorTimerInterval = M2M_DIRECT_INDICATOR_LED_DISCONNECTED_INTERVAL;
				if(debug_uart_ != nullptr)
				{
					_debugState();
				}
				if(disconnectedCallback != nullptr)
				{
					disconnectedCallback();
				}
			}
		}
		if(millis() - receivedLocalActivityTimer > _keepaliveInterval*3) //We've defintely missed an echo
		{
			receivedLocalActivityTimer = millis();
			_echoQuality = m2m._echoQuality >> 1;	//Reduce echo quality
		}
	}
	else if(state == m2mDirectState::disconnected)
	{
		//Disconnected state, try keepalives but also channel hunt if possible
		if(millis() - _localActivityTimer > _keepaliveInterval)
		{
			_createKeepaliveMessage();
			_sendUnicastPacket(_protocolPacketBuffer, _protocolPacketBufferPosition);	//Send quality and keepalive interval is automatically decremented in _sendUnicastPacket if it fails
			_advanceTimers();	//Advance the timers for keepalives
			if(_countBits(linkQuality()) >= M2M_DIRECT_LINK_QUALITY_UPPER_THRESHOLD)	//Assess AND of send and echo quality
			{
				state = m2mDirectState::connected;
				if(_indicatorLedGpio != 255) //Switch on the indicator
				{
					_indicatorTimerInterval = M2M_DIRECT_INDICATOR_LED_CONNECTED_INTERVAL;
					_indicatorOn();
				}
				if(debug_uart_ != nullptr)
				{
					_debugState();
				}
				if(connectedCallback != nullptr)
				{
					connectedCallback();
				}
			}
		}
	}
	if(_pairingButtonGpio != 255) //Check the pairing button
	{
		if(digitalRead(_pairingButtonGpio) == _pairingButtonGpioNc)
		{
			if(_pairingButtonPressTime == 0)
			{
				_pairingButtonPressTime = millis();
			}
			else
			{
				if(millis() - _pairingButtonPressTime > 5000)
				{
					_pairingButtonPressTime = 0;
					if(debug_uart_ != nullptr)
					{
						debug_uart_->print(F("\n\rPairing reset: "));
					}
					if(resetPairing())
					{
						if(debug_uart_ != nullptr)
						{
							debug_uart_->print(F("OK"));
						}
					}
					else
					{
						if(debug_uart_ != nullptr)
						{
							debug_uart_->print(F("failed"));
						}
					}
				}
			}
		}
		else
		{
			_pairingButtonPressTime = 0;
		}
	}
	if(_indicatorLedGpio != 255) //Maintain the indicator LED
	{
		if(_indicatorTimerInterval > 0)
		{
			if(millis() - _indicatorTimer > _indicatorTimerInterval)
			{
				_indicatorTimer = millis();
				if(_indicatorState == true)
				{
					_indicatorOff();	//Turn off the indicator
				}
				else
				{
					_indicatorOn();	//Turn on the indicator
				}
			}
		}
		/*
		else
		{
			if(_indicatorState == false)
			{
				_indicatorOn();	//Turn on the indicator
			}
		}
		*/
	}
}
void ICACHE_FLASH_ATTR m2mDirectClass::_advanceTimers()
{
	_previouslocalActivityTimer = _localActivityTimer;
	_localActivityTimer = millis();
}
void ICACHE_FLASH_ATTR m2mDirectClass::_increaseKeepaliveInterval()
{
	//_keepaliveInterval = _keepaliveInterval * 2;
	_keepaliveInterval = _keepaliveInterval + 100;
	if(_keepaliveInterval > _maximumKeepaliveInterval)
	{
		_keepaliveInterval = _maximumKeepaliveInterval;
	}
}
void ICACHE_FLASH_ATTR m2mDirectClass::_decreaseKeepaliveInterval()
{
	_keepaliveInterval = _keepaliveInterval / 2;
	if(_keepaliveInterval < _minimumKeepaliveInterval)
	{
		_keepaliveInterval = _minimumKeepaliveInterval;
	}
}
void ICACHE_FLASH_ATTR m2mDirectClass::_indicatorOn()
{
	if(_indicatorLedGpioInverted == true)
	{
		digitalWrite(_indicatorLedGpio, LOW);
	}
	else
	{
		digitalWrite(_indicatorLedGpio, HIGH);
	}
	_indicatorState = true;
}
void ICACHE_FLASH_ATTR m2mDirectClass::_indicatorOff()
{
	if(_indicatorLedGpioInverted == true)
	{
		digitalWrite(_indicatorLedGpio, HIGH);
	}
	else
	{
		digitalWrite(_indicatorLedGpio, LOW);
	}
	_indicatorState = false;
}
/*
 *
 *	Register a device as a peer without an encryption key
 *
 */
bool ICACHE_FLASH_ATTR m2mDirectClass::_registerPeer(uint8_t* macaddress, uint8_t channel)
{
	#if defined(ESP8266)
	int result = esp_now_add_peer(macaddress, ESP_NOW_ROLE_COMBO, channel, NULL, 0);
	#elif defined ESP32
	esp_now_peer_info_t newPeer;
	newPeer.peer_addr[0] = (uint8_t) macaddress[0];
	newPeer.peer_addr[1] = (uint8_t) macaddress[1];
	newPeer.peer_addr[2] = (uint8_t) macaddress[2];
	newPeer.peer_addr[3] = (uint8_t) macaddress[3];
	newPeer.peer_addr[4] = (uint8_t) macaddress[4];
	newPeer.peer_addr[5] = (uint8_t) macaddress[5];
	if(WiFi.getMode() == WIFI_STA)
	{
		newPeer.ifidx = WIFI_IF_STA;
	}
	else
	{
		newPeer.ifidx = WIFI_IF_AP;
	}
	newPeer.channel = channel;
	newPeer.encrypt = false;
	int result = esp_now_add_peer(&newPeer);
	#endif
	if(result == ESP_OK)
	{
		if(debug_uart_ != nullptr)
		{
			debug_uart_->printf_P(PSTR("\n\rRegistered unencrypted peer:%02x%02x%02x%02x%02x%02x"), macaddress[0], macaddress[1], macaddress[2], macaddress[3], macaddress[4], macaddress[5]);
		}
		return true;
	}
	else
	{
		if(debug_uart_ != nullptr)
		{
			debug_uart_->printf_P(PSTR("\n\rUnable to register unencrypted peer:%02x%02x%02x%02x%02x%02x"), macaddress[0], macaddress[1], macaddress[2], macaddress[3], macaddress[4], macaddress[5]);
		}
	}
	return false;
}
/*
 *
 *	Register a device as a peer and set an encryption key
 *
 */
bool ICACHE_FLASH_ATTR m2mDirectClass::_registerPeer(uint8_t* macaddress, uint8_t channel, uint8_t* key)
{
	#if defined(ESP8266)
	int result = esp_now_add_peer(macaddress,(uint8_t)ESP_NOW_ROLE_COMBO,(uint8_t)channel, key, ENCRYPTION_KEY_LENGTH);
	#elif defined ESP32
	esp_now_peer_info_t newPeer;
	newPeer.peer_addr[0] = (uint8_t) macaddress[0];
	newPeer.peer_addr[1] = (uint8_t) macaddress[1];
	newPeer.peer_addr[2] = (uint8_t) macaddress[2];
	newPeer.peer_addr[3] = (uint8_t) macaddress[3];
	newPeer.peer_addr[4] = (uint8_t) macaddress[4];
	newPeer.peer_addr[5] = (uint8_t) macaddress[5];
	newPeer.lmk[0] = (uint8_t) key[0];
	newPeer.lmk[1] = (uint8_t) key[1];
	newPeer.lmk[2] = (uint8_t) key[2];
	newPeer.lmk[3] = (uint8_t) key[3];
	newPeer.lmk[4] = (uint8_t) key[4];
	newPeer.lmk[5] = (uint8_t) key[5];
	newPeer.lmk[6] = (uint8_t) key[6];
	newPeer.lmk[7] = (uint8_t) key[7];
	newPeer.lmk[8] = (uint8_t) key[8];
	newPeer.lmk[9] = (uint8_t) key[9];
	newPeer.lmk[10] = (uint8_t) key[10];
	newPeer.lmk[11] = (uint8_t) key[11];
	newPeer.lmk[12] = (uint8_t) key[12];
	newPeer.lmk[13] = (uint8_t) key[13];
	newPeer.lmk[14] = (uint8_t) key[14];
	newPeer.lmk[15] = (uint8_t) key[15];
	if(WiFi.getMode() == WIFI_STA)
	{
		newPeer.ifidx = WIFI_IF_STA;
	}
	else
	{
		newPeer.ifidx = WIFI_IF_AP;
	}
	newPeer.channel = channel;
	newPeer.encrypt = true;
	int result = esp_now_add_peer(&newPeer);
	#endif
	if(result == ESP_OK)
	{
		if(debug_uart_ != nullptr)
		{
			debug_uart_->printf_P(("\n\rRegistered unicast peer:%02x%02x%02x%02x%02x%02x key:%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"), macaddress[0], macaddress[1], macaddress[2], macaddress[3], macaddress[4], macaddress[5], key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7], key[8], key[9], key[10], key[11], key[12], key[13], key[14], key[15]);
		}
		return true;
	}
	else
	{
		if(debug_uart_ != nullptr)
		{
			debug_uart_->printf_P(PSTR("\n\rUnable to register unicast peer:%02x%02x%02x%02x%02x%02x key:%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"), macaddress[0], macaddress[1], macaddress[2], macaddress[3], macaddress[4], macaddress[5], key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7], key[8], key[9], key[10], key[11], key[12], key[13], key[14], key[15]);
		}
	}
	return false;
}
/*
 *
 *	This method scans for the least congested 2.4Ghz channel and uses a simple heuristic to pick the least congested one
 *
 */
uint8_t ICACHE_FLASH_ATTR m2mDirectClass::_leastCongestedChannel()
{
	WiFi.disconnect();
	uint8_t numberOfSsids = WiFi.scanNetworks();
	int16_t rssi[14] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	if(debug_uart_ != nullptr)
	{
		debug_uart_->print(F("\n\rScanning for least congested channel: "));
		debug_uart_->print(numberOfSsids);
		debug_uart_->print(F(" SSIDs found"));
	}
	for(uint8_t ssid = 0; ssid < numberOfSsids; ssid++)
	{
		if(debug_uart_ != nullptr)
		{
			debug_uart_->print(F("\n\r"));
			debug_uart_->print(ssid);
			debug_uart_->print(F(" SSID: "));
			debug_uart_->print(WiFi.SSID(ssid));
			debug_uart_->print(F(" channel: "));
			debug_uart_->print(WiFi.channel(ssid));
			debug_uart_->print(F(" RSSI: "));
			debug_uart_->print(WiFi.RSSI(ssid));
			#if defined(ESP8266)
			debug_uart_->print(F(" hidden: "));
			debug_uart_->print(WiFi.isHidden(ssid));
			#endif
		}
		rssi[WiFi.channel(ssid) - 1]+=(WiFi.RSSI(ssid) > -85 ? WiFi.RSSI(ssid) + 85 : 0);	//Total up the RSSI for each channel, shifted to -85 means 0;
	}
	if(debug_uart_ != nullptr)
	{
		for(uint8_t channel = 0; channel < 14; channel++)
		{
			debug_uart_->print(F("\n\rChannel: "));
			debug_uart_->print(channel+1);
			debug_uart_->print(F(" RSSI congestion: "));
			debug_uart_->print(rssi[channel]);
		}
	}
	WiFi.scanDelete();
	if(rssi[0] < rssi[5] && rssi[0] < rssi[10])
	{
		return 1;
	}
	else if(rssi[5] < rssi[0] && rssi[5] < rssi[10])
	{
		return 6;
	}
	else
	{
		return 11;
	}
	return 1;
}
/*
 *
 *	This method manages the changing of the Wi-Fi channel with some verification that it happened
 *
 */
bool ICACHE_FLASH_ATTR m2mDirectClass::_changeChannel(uint8_t channel)
{
	if(_currentChannel() != channel)
	{
		//Channel 14 is only usable in Japan
		if (channel > 13)
		{
			#if defined(ESP8266)
			wifi_country_t wiFiCountryConfiguration;
			wiFiCountryConfiguration.cc[0] = 'J';
			wiFiCountryConfiguration.cc[1] = 'P';
			wiFiCountryConfiguration.cc[2] = '\0';
			wiFiCountryConfiguration.schan = 1;
			wiFiCountryConfiguration.nchan = 14;
			wiFiCountryConfiguration.policy = WIFI_COUNTRY_POLICY_MANUAL;
			if (wifi_set_country(&wiFiCountryConfiguration) == false)
			{
				if(debug_uart_ != nullptr)
				{
					debug_uart_->print(F("\n\rUnable to set country to JP for channel 14 use"));
				}
				return false;
			}
			#elif defined ESP32
			#endif
		}
		#if defined(ESP8266)
		if(wifi_set_channel(channel))
		#elif defined ESP32
		if(esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE) == ESP_OK)
		#endif
		{
			if(debug_uart_ != nullptr)
			{
				debug_uart_->print(F("\n\rChannel set to : "));
				debug_uart_->print(channel);
			}
			return true;
		}
		else
		{
			if(debug_uart_ != nullptr)
			{
				debug_uart_->print(F("\n\rUnable to set channel to : "));
				debug_uart_->print(channel);
			}
			return false;
		}
	}
	return true;
}
/*
 *
 *	This method initialises the WiFi interface
 *
 */
bool ICACHE_FLASH_ATTR m2mDirectClass::_initialiseWiFi()
{
	#if defined(ESP8266)
	if(WiFi.status() == 7)	//This seems to be the 'not started' status, which isn't documented in the ESP8266 core header files. If you don't start WiFi, no packets will be sent
	#elif defined ESP32
	if(WiFi.getMode() == WIFI_OFF)
	#endif
	{
		if(debug_uart_ != nullptr)
		{
			debug_uart_->print(F("\n\rInitialising WiFi interface: "));
		}
		#if defined(ESP8266)
		if(WiFi.mode(WIFI_STA) == ESP_OK)
		{
			wl_status_t status = WiFi.begin();
			if(status == WL_IDLE_STATUS || status == WL_CONNECTED)
			{
				if(status == WL_IDLE_STATUS)
				{
					WiFi.disconnect();
				}
				if(debug_uart_ != nullptr)
				{
					debug_uart_->print(F("OK"));
				}
			}
			else
			{
				if(debug_uart_ != nullptr)
				{
					debug_uart_->print(F("failed to begin"));
				}
				return false;
			}
		}
		else
		{
			if(debug_uart_ != nullptr)
			{
				debug_uart_->print(F("failed to set as station"));
			}
			return false;
		}
		#elif defined ESP32
		WiFi.begin();							//Start the WiFi
		esp_err_t status = WiFi.mode(WIFI_STA);	//Annoyingly this errors, but then everything works, so can't check for success
		if(debug_uart_ != nullptr)
		{
			debug_uart_->print(F("OK"));
		}
		#endif
	}
	else
	{
		if(debug_uart_ != nullptr)
		{
			debug_uart_->print(F("\n\rWifi already initialised"));
		}
		if(WiFi.status() != WL_CONNECTED)
		{
			WiFi.disconnect();
		}
	}
	#if defined(ESP8266)
	wifi_get_macaddr(STATION_IF, _localMacAddress);
	#elif defined ESP32
	WiFi.macAddress(_localMacAddress);
	if(WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA)
	{
		_localMacAddress[5] = _localMacAddress[5] - 1;			//Decrement the last octet of the MAC address, it is incremented in AP mode
	}
	#endif
	if(debug_uart_ != nullptr)
	{
		debug_uart_->printf_P(PSTR("\n\rMAC address (STATION_IF):%02x%02x%02x%02x%02x%02x"), _localMacAddress[0], _localMacAddress[1], _localMacAddress[2], _localMacAddress[3], _localMacAddress[4], _localMacAddress[5]);
		if(WiFi.status() == WL_CONNECTED)
		{
			debug_uart_->print(F("\n\rIP address: "));
			debug_uart_->print(WiFi.localIP());
		}
	}
	if(WiFi.status() == WL_CONNECTED)
	{
		if(debug_uart_ != nullptr)
		{
			debug_uart_->print(F("\n\rCommunicating on AP channel: "));
			debug_uart_->print(_currentChannel());
		}
		_pairingChannel = _currentChannel();
		_communicationChannel = _currentChannel();
	}
	else
	{
		if(debug_uart_ != nullptr)
		{
			debug_uart_->print(F("\n\rm2mDirect pairing on channel: "));
			debug_uart_->print(_pairingChannel);
			debug_uart_->print(F("\n\rm2mDirect communicating on channel: "));
		}
		if(_communicationChannel !=0)
		{
			if(debug_uart_ != nullptr)
			{
				debug_uart_->print(_communicationChannel);
			}
		}
		else
		{
			if(debug_uart_ != nullptr)
			{
				debug_uart_->print(F("automatic"));
			}
			_communicationChannel = _leastCongestedChannel();
			if(debug_uart_ != nullptr)
			{
				debug_uart_->print(F("\n\rAutomatic channel suggestion: "));
				debug_uart_->print(_communicationChannel);
			}
		}
	}
	return true;
}
/*
 *
 *	This method sets up ESP-Now with peers, callbacks and so on as necessary
 *
 */
bool ICACHE_FLASH_ATTR m2mDirectClass::_initialiseEspNow(uint8_t channel)
{
	if(_changeChannel(channel) == true)
	{
		WiFi.disconnect();
		if(esp_now_init() == ESP_OK)
		{
			if(debug_uart_ != nullptr)
			{
				debug_uart_->print(F("\n\rESP-Now initialised on channel: "));
				debug_uart_->print(_currentChannel());
			}
			#if defined(ESP8266)
			if(esp_now_set_self_role(ESP_NOW_ROLE_COMBO) == ESP_OK)
			{
				if(_initialiseEspNowCallbacks() == true)
				{
					//if(esp_now_add_peer(_broadcastMacAddress,(uint8_t)ESP_NOW_ROLE_COMBO,(uint8_t)channel, NULL, 0) == ESP_OK)
					if(_registerPeer(_broadcastMacAddress, channel))
					{
						return true;
					}
				}
			}
			else
			{
				if(debug_uart_ != nullptr)
				{
					debug_uart_->print(F("\n\rUnable to set ESP-Now role"));
				}
			}
			#elif defined ESP32
			if(_initialiseEspNowCallbacks() == true)
			{
				if(_registerPeer(_broadcastMacAddress, channel))
				{
					return true;
				}
			}
			#endif
		}
		else
		{
			if(debug_uart_ != nullptr)
			{
				debug_uart_->print(F("\n\rUnable to initialise ESP-Now"));
			}
		}
	}
	if(debug_uart_ != nullptr)
	{
		debug_uart_->print(F("\n\rm2mDirect failed to initialise"));
	}
	return false;
}
/*
 *
 *	This method configures the ESP-Now callbacks needed to handle incoming messages and verify receipt of outgoing messages
 *
 */
bool ICACHE_FLASH_ATTR m2mDirectClass::_initialiseEspNowCallbacks()
{
	if(debug_uart_ != nullptr)
	{
		debug_uart_->print(F("\n\rCreating receive callback for communicating with ESP-Now peers: "));
	}
	//The receive callback is a somewhat length lambda function
	#if defined(ESP8266)
	if(esp_now_register_recv_cb([](uint8_t *macAddress, uint8_t *receivedMessage, uint8_t receivedMessageLength) {
	#elif defined ESP32
	if(esp_now_register_recv_cb([](const uint8_t *macAddress, const uint8_t *receivedMessage, int receivedMessageLength) {
	#endif
		//Copy the received CRC32
		uint32_t receivedCrc = receivedMessage[receivedMessageLength - 1];
		receivedCrc+=receivedMessage[receivedMessageLength - 2] << 8;
		receivedCrc+=receivedMessage[receivedMessageLength - 3] << 16;
		receivedCrc+=receivedMessage[receivedMessageLength - 4] << 24;
		#ifdef M2M_DIRECT_DEBUG_RECEIVE
		if(m2m.debug_uart_ != nullptr)
		{
			m2m.debug_uart_->printf_P(PSTR("\n\rRX %03u bytes from:%02x%02x%02x%02x%02x%02x "), receivedMessageLength, macAddress[0], macAddress[1], macAddress[2], macAddress[3], macAddress[4], macAddress[5]);
			m2m._printPacketDescription(receivedMessage[0]);
			//m2m.debug_uart_->printf_P(PSTR(" CRC:%08x "),  receivedCrc);
		}
		#endif
		//Calculate the received CRC32
		CRC32 crc;
		crc.add((uint8_t*)receivedMessage, receivedMessageLength - 4);
		//Check the CRC32
		if(crc.calc() == receivedCrc)
		{
			#ifdef M2M_DIRECT_DEBUG_RECEIVE
			if(m2m.debug_uart_ != nullptr)
			{
				m2m.debug_uart_->print(F(" valid"));
			}
			#endif
			//Pairing messages are the first stage in setting up a connection, sent broadcast
			//These will expose at least one of the encryption keys
			if(receivedMessage[0] == M2M_DIRECT_PAIRING_FLAG)
			{
				//Debug output
				if(m2m.debug_uart_ != nullptr)
				{
					m2m.debug_uart_->printf_P(PSTR("\n\rPairing message on channel:%i from %02x%02x%02x%02x%02x%02x\n\r\tGlobal encryption key:%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n\r\tLocal encryption key:%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"),
						receivedMessage[1],//Channel
						receivedMessage[2],//Remote MAC address
						receivedMessage[3],
						receivedMessage[4],
						receivedMessage[5],
						receivedMessage[6],
						receivedMessage[7],
						receivedMessage[8],//Primary encryption key
						receivedMessage[9],
						receivedMessage[10],
						receivedMessage[11],
						receivedMessage[12],
						receivedMessage[13],
						receivedMessage[14],
						receivedMessage[15],
						receivedMessage[16],
						receivedMessage[17],
						receivedMessage[18],
						receivedMessage[19],
						receivedMessage[20],
						receivedMessage[21],
						receivedMessage[22],
						receivedMessage[23],
						receivedMessage[24],//Local encryption key
						receivedMessage[25],
						receivedMessage[26],
						receivedMessage[27],
						receivedMessage[28],
						receivedMessage[29],
						receivedMessage[30],
						receivedMessage[31],
						receivedMessage[32],
						receivedMessage[33],
						receivedMessage[34],
						receivedMessage[35],
						receivedMessage[36],
						receivedMessage[37],
						receivedMessage[38],
						receivedMessage[39]
					);
					if(receivedMessage[40] > 0)	//There is a name
					{
						m2m.debug_uart_->printf_P(PSTR("\n\r\tName:'%s' length:%u"), &receivedMessage[41], receivedMessage[40]);
					}
				}
				//The normal state of things, one of the pair will get there first
				if(m2m.state == m2mDirectState::pairing)
				{
					//Copy the remote MAC address
					memcpy(m2m._remoteMacAddress, &receivedMessage[2], MAC_ADDRESS_LENGTH);
					//Copy the remote name
					if(m2m.remoteDeviceName == nullptr)
					{
						if(receivedMessage[40] > 0)	//There is a name
						{
							m2m.remoteDeviceName = new char[receivedMessage[40] + 1];
							memcpy(m2m.remoteDeviceName, &receivedMessage[41], receivedMessage[40]);
							m2m.remoteDeviceName[receivedMessage[40]] = 0;	//Null terminate this string
						}
					}
					if(m2m._tieBreak(m2m._remoteMacAddress, m2m._localMacAddress)) //Do a tie break based on MAC address
					{
						if(memcmp(m2m._primaryEncryptionKey,&receivedMessage[8],ENCRYPTION_KEY_LENGTH !=0) ||
							memcmp(m2m._localEncryptionKey,&receivedMessage[24],ENCRYPTION_KEY_LENGTH !=0))
						{
							if(m2m.debug_uart_ != nullptr)
							{
								m2m.debug_uart_->print(F("\n\rRemote device wins tie, using its keys"));
							}
							//Copy the expected communication channel, overriding this device's choice
							m2m._communicationChannel = receivedMessage[1];
							//Copy the global encryption key, overriding this device's choice
							memcpy(m2m._primaryEncryptionKey, &receivedMessage[8], ENCRYPTION_KEY_LENGTH);
							//Copy the local encryption key, overriding this device's choice
							memcpy(m2m._localEncryptionKey, &receivedMessage[24], ENCRYPTION_KEY_LENGTH);
						}
						else
						{
							if(m2m.debug_uart_ != nullptr)
							{
								m2m.debug_uart_->print(F("\n\rRemote device won tie, already have its keys"));
							}
						}
						//Change state to confirm pairing with other device
						if(m2m.debug_uart_ != nullptr)
						{
							m2m.debug_uart_->print(F("\n\rPaired"));
						}
						if(m2m._encyptionEnabled == true)
						{
							if(m2m._setPrimaryEncryptionKey() == true)	//Use the advertised primary encryption key
							{
								if(m2m._registerPeer(m2m._remoteMacAddress, m2m._communicationChannel, m2m._localEncryptionKey) == true)
								{
									//Move on to the paired state and start sending pairing ACKs
									m2m._createPairingAckMessage();
									m2m.state = m2mDirectState::paired;
									m2m._indicatorTimerInterval = M2M_DIRECT_INDICATOR_LED_PAIRED_INTERVAL;
									if(m2m.debug_uart_ != nullptr)
									{
										m2m._debugState();
									}
									if(m2m.pairedCallback != nullptr)
									{
										m2m.pairedCallback();
									}
								}
							}
						}
						else
						{
							if(m2m._registerPeer(m2m._remoteMacAddress, m2m._communicationChannel))
							{
								//Move on to the paired state and start sending pairing ACKs
								m2m._createPairingAckMessage();
								m2m.state = m2mDirectState::paired;
								m2m._indicatorTimerInterval = M2M_DIRECT_INDICATOR_LED_PAIRED_INTERVAL;
								if(m2m.debug_uart_ != nullptr)
								{
									m2m._debugState();
								}
								if(m2m.pairedCallback != nullptr)
								{
									m2m.pairedCallback();
								}
							}
						}
					}
					else
					{
						if(m2m.debug_uart_ != nullptr)
						{
							m2m.debug_uart_->print(F("\n\rLocal device wins tie"));
						}
					}
				}
				else if(m2m.state == m2mDirectState::paired)
				{
					if(m2m.debug_uart_ != nullptr)
					{
						m2m.debug_uart_->print(F("\n\rIgnoring pairing message, already paired"));
					}
				}
				else if(m2m.state == m2mDirectState::connected)
				{
					if(m2m.debug_uart_ != nullptr)
					{
						m2m.debug_uart_->print(F("\n\rIgnoring pairing message, already connected"));
					}
				}
				else
				{
					if(m2m.debug_uart_ != nullptr)
					{
						m2m.debug_uart_->print(F("\n\rIgnoring unexpected message"));
					}
				}
			}
			//Pairing ACKs show that at least one end has both encryption keys and the tie is broken
			else if(receivedMessage[0] == M2M_DIRECT_PAIRING_ACK_FLAG)
			{
				//Debug output
				if(m2m.debug_uart_ != nullptr)
				{
					m2m.debug_uart_->printf_P(PSTR("\n\rPairing ACK message on channel:%u from %02x%02x%02x%02x%02x%02x\r\n\tGlobal Key:%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x for %02x%02x%02x%02x%02x%02x\r\n\tLocal Key:%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"),
						receivedMessage[1],	//Channel
						receivedMessage[2],	//Remote MAC address
						receivedMessage[3],
						receivedMessage[4],
						receivedMessage[5],
						receivedMessage[6],
						receivedMessage[7],
						receivedMessage[8], //Local Mac address
						receivedMessage[9],
						receivedMessage[10],
						receivedMessage[11],
						receivedMessage[12],
						receivedMessage[13],
						receivedMessage[14],//Global encryption key
						receivedMessage[15],
						receivedMessage[16],
						receivedMessage[17],
						receivedMessage[18],
						receivedMessage[19],
						receivedMessage[20],
						receivedMessage[21],
						receivedMessage[22],
						receivedMessage[23],
						receivedMessage[24],
						receivedMessage[25],
						receivedMessage[26],
						receivedMessage[27],
						receivedMessage[28],
						receivedMessage[29],
						receivedMessage[30], //Local encryption key
						receivedMessage[31],
						receivedMessage[32],
						receivedMessage[33],
						receivedMessage[34],
						receivedMessage[35],
						receivedMessage[36],
						receivedMessage[37],
						receivedMessage[38],
						receivedMessage[39],
						receivedMessage[40],
						receivedMessage[41],
						receivedMessage[42],
						receivedMessage[43],
						receivedMessage[44],
						receivedMessage[45]
					);
					if(receivedMessage[46] > 0)	//There is a name
					{
						m2m.debug_uart_->printf_P(PSTR("\n\r\tName:'%s' length:%u"), &receivedMessage[47], receivedMessage[46]);
					}
				}
				//This node sent the first pairing message and has had a pairing ACK in response
				if(m2m.state == m2mDirectState::pairing)
				{
					if(m2m._remoteMacAddressSet() == false)
					{
						//Copy the remote MAC address
						memcpy(m2m._remoteMacAddress, &receivedMessage[2], MAC_ADDRESS_LENGTH);
						//Copy the remote name
						if(m2m.remoteDeviceName == nullptr)
						{
							if(receivedMessage[46] > 0)	//There is a name
							{
								m2m.remoteDeviceName = new char[receivedMessage[46] + 1];
								memcpy(m2m.remoteDeviceName, &receivedMessage[47], receivedMessage[46]);
								m2m.remoteDeviceName[receivedMessage[46]] = 0;	//Null terminate this string
							}
						}
					}
					if(m2m._tieBreak(m2m._localMacAddress, (uint8_t*)&receivedMessage[2]) && //Do a tie break based on MAC address
						//Matches the expected communication channel
						receivedMessage[1] == m2m._communicationChannel &&
						//Matches the remote MAC address
						memcmp(&receivedMessage[2], m2m._remoteMacAddress, MAC_ADDRESS_LENGTH) == 0 &&
						//Matches the local MAC address
						memcmp(&receivedMessage[8], m2m._localMacAddress, MAC_ADDRESS_LENGTH) == 0 &&
						//Matches the global encryption key
						memcmp(&receivedMessage[14], m2m._primaryEncryptionKey, ENCRYPTION_KEY_LENGTH) == 0 &&
						//Matches the local encryption key
						memcmp(&receivedMessage[30], m2m._localEncryptionKey, ENCRYPTION_KEY_LENGTH) == 0
					)
					{
						if(m2m._encyptionEnabled == true)
						{
							if(m2m._setPrimaryEncryptionKey() == true)	//Use the advertised primary encryption key
							{
								if(m2m._registerPeer(m2m._remoteMacAddress, m2m._communicationChannel, m2m._localEncryptionKey) == true)
								{
									//Both ends match, move to paired
									if(m2m.debug_uart_ != nullptr)
									{
										m2m.debug_uart_->print(F("\n\rPairing confirmed"));
									}
									//Move on to the paired state and start sending pairing ACKs
									m2m._createPairingAckMessage();
									m2m.state = m2mDirectState::paired;
									m2m._indicatorTimerInterval = M2M_DIRECT_INDICATOR_LED_PAIRED_INTERVAL;
									if(m2m.debug_uart_ != nullptr)
									{
										m2m._debugState();
									}
									if(m2m.pairedCallback != nullptr)
									{
										m2m.pairedCallback();
									}
								}
							}
						}
						else
						{
							if(m2m._registerPeer(m2m._remoteMacAddress, m2m._communicationChannel))
							{
								//Both ends match, move to Paired
								if(m2m.debug_uart_ != nullptr)
								{
									m2m.debug_uart_->print(F("\n\rPairing confirmed"));
								}
								//Move on to the paired state and start sending pairing ACKs
								m2m._createPairingAckMessage();
								m2m.state = m2mDirectState::paired;
								m2m._indicatorTimerInterval = M2M_DIRECT_INDICATOR_LED_PAIRED_INTERVAL;
								if(m2m.debug_uart_ != nullptr)
								{
									m2m._debugState();
								}
								if(m2m.pairedCallback != nullptr)
								{
									m2m.pairedCallback();
								}
							}
						}
					}
					else
					{
						if(m2m.debug_uart_ != nullptr)
						{
							m2m.debug_uart_->print(F("\n\rUnexpected pairing ACK contents"));
							if(receivedMessage[1] != m2m._communicationChannel)
							{
								m2m.debug_uart_->print(F("\n\rChannel mismatch"));
							}
							if(memcmp(&receivedMessage[2], m2m._remoteMacAddress, MAC_ADDRESS_LENGTH) != 0)
							{
								m2m.debug_uart_->printf_P(PSTR("\n\rRemote MAC address mismatch, wanted %02x%02x%02x%02x%02x%02x have %02x%02x%02x%02x%02x%02x"),
									m2m._remoteMacAddress[0],
									m2m._remoteMacAddress[1],
									m2m._remoteMacAddress[2],
									m2m._remoteMacAddress[3],
									m2m._remoteMacAddress[4],
									m2m._remoteMacAddress[5],
									receivedMessage[2],
									receivedMessage[3],
									receivedMessage[4],
									receivedMessage[5],
									receivedMessage[6],
									receivedMessage[7]
								);
							}
							if(memcmp(&receivedMessage[8], m2m._localEncryptionKey, ENCRYPTION_KEY_LENGTH) != 0)
							{
								m2m.debug_uart_->print(F("\n\rLocal encryption key mismatch"));
							}
							if(memcmp(&receivedMessage[24], m2m._localMacAddress, MAC_ADDRESS_LENGTH) != 0)
							{
								m2m.debug_uart_->printf_P(PSTR("\n\rLocal MAC address mismatch, wanted %02x%02x%02x%02x%02x%02x have %02x%02x%02x%02x%02x%02x"),
									m2m._localMacAddress[0],
									m2m._localMacAddress[1],
									m2m._localMacAddress[2],
									m2m._localMacAddress[3],
									m2m._localMacAddress[4],
									m2m._localMacAddress[5],
									receivedMessage[24],
									receivedMessage[25],
									receivedMessage[26],
									receivedMessage[27],
									receivedMessage[28],
									receivedMessage[29]
								);
							}
							if(memcmp(&receivedMessage[30], m2m._localEncryptionKey, ENCRYPTION_KEY_LENGTH) != 0)
							{
								m2m.debug_uart_->print(F("\n\rLocal encryption key"));
							}
						}
					}
				}
				else if(m2m.state == m2mDirectState::paired)
				{
					if(	//Matches the expected communication channel
						receivedMessage[1] == m2m._communicationChannel &&
						//Matches the remote MAC address
						memcmp(&receivedMessage[2], m2m._remoteMacAddress, MAC_ADDRESS_LENGTH) == 0 &&
						//Matches the local MAC address
						memcmp(&receivedMessage[8], m2m._localMacAddress, MAC_ADDRESS_LENGTH) == 0 &&
						//Matches the remote encryption key
						memcmp(&receivedMessage[14], m2m._primaryEncryptionKey, ENCRYPTION_KEY_LENGTH) == 0 &&
						//Matches the local encryption key
						memcmp(&receivedMessage[30], m2m._localEncryptionKey, ENCRYPTION_KEY_LENGTH) == 0
					)
					{
						if(m2m._tieBreak(m2m._localMacAddress, (uint8_t*)&receivedMessage[2]))
						{
							//Both ends match, move to connecting
							if(m2m.debug_uart_ != nullptr)
							{
								m2m.debug_uart_->print(F("\n\rTie winner, connecting"));
							}
							m2m.state = m2mDirectState::connecting;
							m2m._indicatorTimerInterval = M2M_DIRECT_INDICATOR_LED_CONNECTING_INTERVAL;
							if(m2m.debug_uart_ != nullptr)
							{
								m2m._debugState();
							}
						}
						else
						{
							if(m2m.debug_uart_ != nullptr)
							{
								m2m.debug_uart_->print(F("\n\rTie loser, waiting for connection"));
							}
						}
					}
					else
					{
						if(m2m.debug_uart_ != nullptr)
						{
							m2m.debug_uart_->print(F("\n\rPairing ACK doesn't match"));
						}
					}
				}
				else if(m2m.state == m2mDirectState::connecting)
				{
					if(m2m.debug_uart_ != nullptr)
					{
						m2m.debug_uart_->print(F("\n\rIgnoring pairing ACK message, already connecting"));
					}
				}
				else if(m2m.state == m2mDirectState::connected)
				{
					if(m2m.debug_uart_ != nullptr)
					{
						m2m.debug_uart_->print(F("\n\rIgnoring pairing ACK message, already connected"));
					}
				}
				else
				{
					if(m2m.debug_uart_ != nullptr)
					{
						m2m.debug_uart_->print(F("\n\rIgnoring pairing ACK message, unexpected state"));
					}
				}
			}
			else if(receivedMessage[0] == M2M_DIRECT_KEEPALIVE_FLAG)
			{
				//Extract the local/remote activity timers for echo quality calculations
				m2m._remoteActivityTimer  =	receivedMessage[14] << 24;
				m2m._remoteActivityTimer+=receivedMessage[15] << 16;
				m2m._remoteActivityTimer+=receivedMessage[16] << 8;
				m2m._remoteActivityTimer+=receivedMessage[17];
				m2m.receivedLocalActivityTimer = receivedMessage[18] << 24;
				m2m.receivedLocalActivityTimer+=receivedMessage[19] << 16;
				m2m.receivedLocalActivityTimer+=receivedMessage[20] << 8;
				m2m.receivedLocalActivityTimer+=receivedMessage[21];
				//Reduce echo quality
				m2m._echoQuality = m2m._echoQuality >> 1;
				if(m2m.state == m2mDirectState::pairing) //Getting here implies pairing failed
				{
					if(m2m.debug_uart_ != nullptr)
					{
						m2m.debug_uart_->print(F("\n\rPairing failed"));
					}
				}
				//Normal state of affairs. Start connecting with keepalives, which don't include keys
				else if(m2m.state == m2mDirectState::paired)
				{
					if(
						//Match the expected communication channel
						receivedMessage[1] == m2m._communicationChannel &&
						//Matches the remote MAC address
						memcmp(&receivedMessage[2], m2m._remoteMacAddress, MAC_ADDRESS_LENGTH) == 0 &&
						//Matches the local MAC address
						memcmp(&receivedMessage[8], m2m._localMacAddress, MAC_ADDRESS_LENGTH) == 0
					)
					{
						if(m2m.debug_uart_ != nullptr)
						{
							m2m.debug_uart_->print(F("\n\rPaired, connecting"));
						}
						m2m.state = m2mDirectState::connecting;
						m2m._indicatorTimerInterval = M2M_DIRECT_INDICATOR_LED_CONNECTING_INTERVAL;
						if(m2m.debug_uart_ != nullptr)
						{
							m2m._debugState();
						}
					}
					else
					{
						if(m2m.debug_uart_ != nullptr)
						{
							m2m.debug_uart_->print(F(" unexpected contents"));
						}
					}
				}
				else if(m2m.state == m2mDirectState::connecting || m2m.state == m2mDirectState::connected || m2m.state == m2mDirectState::disconnected)
				{
					if(
						//Match the expected communication channel
						receivedMessage[1] == m2m._communicationChannel &&
						//Matches the remote MAC address
						memcmp(&receivedMessage[2], m2m._remoteMacAddress, MAC_ADDRESS_LENGTH) == 0 &&
						//Matches the local MAC address
						memcmp(&receivedMessage[8], m2m._localMacAddress, MAC_ADDRESS_LENGTH) == 0
					)
					{
						if(m2m.receivedLocalActivityTimer == m2m._previouslocalActivityTimer)
						{
							m2m._echoQuality = m2m._echoQuality | 0x80000000; //Improve echo quality
							if(m2m.debug_uart_ != nullptr)
							{
								m2m.debug_uart_->print(F(" in sequence"));
							}
						}
						else
						{
							if(m2m.debug_uart_ != nullptr)
							{
								m2m.debug_uart_->print(F(" some missed, off by "));
								m2m.debug_uart_->print(m2m._previouslocalActivityTimer - m2m.receivedLocalActivityTimer);
								m2m.debug_uart_->print(F("ms"));
							}
						}
					}
					else
					{
						if(m2m.debug_uart_ != nullptr)
						{
							m2m.debug_uart_->print(F(" unexpected contents"));
						}
					}
				}
				else
				{
					if(m2m.debug_uart_ != nullptr)
					{
						m2m.debug_uart_->print(F(" unexpected in state "));
						m2m._printCurrentState();
					}
				}
			}
			else if(receivedMessage[0] == M2M_DIRECT_DATA_FLAG)
			{
				if(m2m._receivedPacketBuffer[1] == 0)
				{
					memcpy(m2m._receivedPacketBuffer, receivedMessage, receivedMessageLength);
					m2m._dataReceived = true;
					if(m2m.debug_uart_ != nullptr)
					{
						m2m.debug_uart_->printf_P(PSTR(" %u fields"),m2m._receivedPacketBuffer[1]);
					}
				}
				else
				{
					if(m2m.debug_uart_ != nullptr)
					{
						m2m.debug_uart_->print(F("\n\rReceived message discarded"));
					}
				}
			}
			else
			{
				if(m2m.debug_uart_ != nullptr)
				{
					m2m.debug_uart_->printf_P(PSTR("\n\rUnknown message type %i"),receivedMessage[0]);
					m2m.debug_uart_->print(F("\n\rData: "));
					for (int i = 0; i < receivedMessageLength; i++) {
						if(receivedMessage[i] < 0x10)
						{
							m2m.debug_uart_->print('0');
						}
						m2m.debug_uart_->print(receivedMessage[i], HEX);
						m2m.debug_uart_->print(' ');
					}
				}
			}
		}
		#ifdef M2M_DIRECT_DEBUG_SEND
		else
		{
			if(m2m.debug_uart_ != nullptr)
			{
				m2m.debug_uart_->printf_P(PSTR(" CRC:%08x "),  receivedCrc);
				m2m.debug_uart_->printf("not valid, calculated CRC %08x",crc.calc());
			}
		}
		#endif
	}) == ESP_OK)
	{
		if(debug_uart_ != nullptr)
		{
			m2m.debug_uart_->print(F("OK"));
		}
	}
	else
	{
		if(debug_uart_ != nullptr)
		{
			m2m.debug_uart_->print(F("Failed"));
		}
		return false;
	}
	if(debug_uart_ != nullptr)
	{
		debug_uart_->print(F("\n\rCreating send callback for communicating with ESP-Now peers: "));
	}
	#if defined(ESP8266)
	if(esp_now_register_send_cb([](uint8_t* macAddress, uint8_t status) {
	#elif defined ESP32
	if(esp_now_register_send_cb([](const uint8_t* macAddress, esp_now_send_status_t status) {
	#endif
		if(m2m._waitingForSendCallback == true &&
			macAddress[0] == m2m._remoteMacAddress[0] &&
			macAddress[1] == m2m._remoteMacAddress[1] &&
			macAddress[2] == m2m._remoteMacAddress[2] &&
			macAddress[3] == m2m._remoteMacAddress[3] &&
			macAddress[4] == m2m._remoteMacAddress[4] &&
			macAddress[5] == m2m._remoteMacAddress[5]
		)
		{
		  if(status == ESP_OK)
		  {
			m2m._waitingForSendCallback = false;
		  }
		}
	}) == ESP_OK)
	{
		if(debug_uart_ != nullptr)
		{
			m2m.debug_uart_->print(F("OK"));
		}
	}
	else
	{
		if(debug_uart_ != nullptr)
		{
			m2m.debug_uart_->print(F("Failed"));
		}
		return false;
	}
	return true;
}
/*
 *
 *	Set zeroes for the encryption keys
 *
 */
void ICACHE_FLASH_ATTR m2mDirectClass::_clearEncryptionKeys()
{
	for(uint8_t index = 0; index < 16; index++)
	{
		_primaryEncryptionKey[index] = 0;
		_localEncryptionKey[index] = 0;
	}
	if(debug_uart_ != nullptr)
	{
		debug_uart_->print(F("\n\rCleared encryption keys"));
	}
}
/*
 *
 *	Set random numbers for the encryption keys
 *
 */
void ICACHE_FLASH_ATTR m2mDirectClass::_chooseEncryptionKeys()
{
	#if defined(ESP8266)
	//This is the hardware random number generator on the ESP8266
	uint32_t random0 = *(volatile uint32_t *)0x3FF20E44;
	uint32_t random1 = *(volatile uint32_t *)0x3FF20E44;
	uint32_t random2 = *(volatile uint32_t *)0x3FF20E44;
	uint32_t random3 = *(volatile uint32_t *)0x3FF20E44;
	uint32_t random4 = *(volatile uint32_t *)0x3FF20E44;
	uint32_t random5 = *(volatile uint32_t *)0x3FF20E44;
	uint32_t random6 = *(volatile uint32_t *)0x3FF20E44;
	uint32_t random7 = *(volatile uint32_t *)0x3FF20E44;
	#elif defined ESP32
	uint32_t random0 = esp_random();
	uint32_t random1 = esp_random();
	uint32_t random2 = esp_random();
	uint32_t random3 = esp_random();
	uint32_t random4 = esp_random();
	uint32_t random5 = esp_random();
	uint32_t random6 = esp_random();
	uint32_t random7 = esp_random();
	#endif
	_primaryEncryptionKey[0] = (random0 & 0xff000000) >> 24;
	_primaryEncryptionKey[1] = (random0 & 0x00ff0000) >> 16;
	_primaryEncryptionKey[2] = (random0 & 0x0000ff00) >> 8;
	_primaryEncryptionKey[3] = (random0 & 0x000000ff);
	_primaryEncryptionKey[4] = (random1 & 0xff000000) >> 24;
	_primaryEncryptionKey[5] = (random1 & 0x00ff0000) >> 16;
	_primaryEncryptionKey[6] = (random1 & 0x0000ff00) >> 8;
	_primaryEncryptionKey[7] = (random1 & 0x000000ff);
	_primaryEncryptionKey[8] = (random2 & 0xff000000) >> 24;
	_primaryEncryptionKey[9] = (random2 & 0x00ff0000) >> 16;
	_primaryEncryptionKey[10] = (random2 & 0x0000ff00) >> 8;
	_primaryEncryptionKey[11] = (random2 & 0x000000ff);
	_primaryEncryptionKey[12] = (random3 & 0xff000000) >> 24;
	_primaryEncryptionKey[13] = (random3 & 0x00ff0000) >> 16;
	_primaryEncryptionKey[14] = (random3 & 0x0000ff00) >> 8;
	_primaryEncryptionKey[15] = (random3 & 0x000000ff);
	_localEncryptionKey[0] = (random4 & 0xff000000) >> 24;
	_localEncryptionKey[1] = (random4 & 0x00ff0000) >> 16;
	_localEncryptionKey[2] = (random4 & 0x0000ff00) >> 8;
	_localEncryptionKey[3] = (random4 & 0x000000ff);
	_localEncryptionKey[4] = (random5 & 0xff000000) >> 24;
	_localEncryptionKey[5] = (random5 & 0x00ff0000) >> 16;
	_localEncryptionKey[6] = (random5 & 0x0000ff00) >> 8;
	_localEncryptionKey[7] = (random5 & 0x000000ff);
	_localEncryptionKey[8] = (random6 & 0xff000000) >> 24;
	_localEncryptionKey[9] = (random6 & 0x00ff0000) >> 16;
	_localEncryptionKey[10] = (random6 & 0x0000ff00) >> 8;
	_localEncryptionKey[11] = (random6 & 0x000000ff);
	_localEncryptionKey[12] = (random7 & 0xff000000) >> 24;
	_localEncryptionKey[13] = (random7 & 0x00ff0000) >> 16;
	_localEncryptionKey[14] = (random7 & 0x0000ff00) >> 8;
	_localEncryptionKey[15] = (random7 & 0x000000ff);
	if(debug_uart_ != nullptr)
	{
		debug_uart_->printf_P(PSTR("\n\rChose primary encryption key:%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"),
			_primaryEncryptionKey[0],
			_primaryEncryptionKey[1],
			_primaryEncryptionKey[2],
			_primaryEncryptionKey[3],
			_primaryEncryptionKey[4],
			_primaryEncryptionKey[5],
			_primaryEncryptionKey[6],
			_primaryEncryptionKey[7],
			_primaryEncryptionKey[8],
			_primaryEncryptionKey[9],
			_primaryEncryptionKey[10],
			_primaryEncryptionKey[11],
			_primaryEncryptionKey[12],
			_primaryEncryptionKey[13],
			_primaryEncryptionKey[14],
			_primaryEncryptionKey[15]
			);
		debug_uart_->printf_P(PSTR("\n\rChose local encryption key:%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"),
			_localEncryptionKey[0],
			_localEncryptionKey[1],
			_localEncryptionKey[2],
			_localEncryptionKey[3],
			_localEncryptionKey[4],
			_localEncryptionKey[5],
			_localEncryptionKey[6],
			_localEncryptionKey[7],
			_localEncryptionKey[8],
			_localEncryptionKey[9],
			_localEncryptionKey[10],
			_localEncryptionKey[11],
			_localEncryptionKey[12],
			_localEncryptionKey[13],
			_localEncryptionKey[14],
			_localEncryptionKey[15]
			);
	}
}
/*
 *
 *	Set the primary encyption key
 *
 */
bool ICACHE_FLASH_ATTR m2mDirectClass::_setPrimaryEncryptionKey()
{
	#if defined (ESP8266)
	if(esp_now_set_kok(_primaryEncryptionKey, ENCRYPTION_KEY_LENGTH) == ESP_OK)
	#elif defined ESP32
	if(esp_now_set_pmk(_primaryEncryptionKey) == ESP_OK)
	#endif
	{
		if(debug_uart_ != nullptr)
		{
			debug_uart_->printf_P(PSTR("\n\rSet primary encryption key:%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"),
				_primaryEncryptionKey[0],
				_primaryEncryptionKey[1],
				_primaryEncryptionKey[2],
				_primaryEncryptionKey[3],
				_primaryEncryptionKey[4],
				_primaryEncryptionKey[5],
				_primaryEncryptionKey[6],
				_primaryEncryptionKey[7],
				_primaryEncryptionKey[8],
				_primaryEncryptionKey[9],
				_primaryEncryptionKey[10],
				_primaryEncryptionKey[11],
				_primaryEncryptionKey[12],
				_primaryEncryptionKey[13],
				_primaryEncryptionKey[14],
				_primaryEncryptionKey[15]
				);
		}
		return true;
	}
	else
	{
		if(debug_uart_ != nullptr)
		{
			debug_uart_->print(F("\n\rFailed to set primary encryption key"));
		}
	}
	return false;
}
/*
 *
 *	This method builds the pairing message
 *
 */
void ICACHE_FLASH_ATTR m2mDirectClass::_createPairingMessage()
{
	_protocolPacketBufferPosition = 0;
	//Add packet flag
	_protocolPacketBuffer[_protocolPacketBufferPosition++] = M2M_DIRECT_PAIRING_FLAG;
	//Add expected communication channel
	_protocolPacketBuffer[_protocolPacketBufferPosition++] = _communicationChannel;
	//Add local MAC address
	memcpy(&_protocolPacketBuffer[_protocolPacketBufferPosition], _localMacAddress, MAC_ADDRESS_LENGTH);
	_protocolPacketBufferPosition+=MAC_ADDRESS_LENGTH;
	//Add global encryption key
	memcpy(&_protocolPacketBuffer[_protocolPacketBufferPosition], _primaryEncryptionKey, ENCRYPTION_KEY_LENGTH);
	_protocolPacketBufferPosition+=ENCRYPTION_KEY_LENGTH;
	//Add local encryption key
	memcpy(&_protocolPacketBuffer[_protocolPacketBufferPosition], _localEncryptionKey, ENCRYPTION_KEY_LENGTH);
	_protocolPacketBufferPosition+=ENCRYPTION_KEY_LENGTH;
	//Add local name if there is one
	if(localDeviceName != nullptr)
	{
		//Add the name length
		_protocolPacketBuffer[_protocolPacketBufferPosition++] = strlen(localDeviceName);
		//Add the name
		memcpy(&_protocolPacketBuffer[_protocolPacketBufferPosition], localDeviceName, strlen(localDeviceName));
		_protocolPacketBufferPosition+=strlen(localDeviceName);
	}
	else
	{
		//Add the name length of zero to signify no name
		_protocolPacketBuffer[_protocolPacketBufferPosition++] = 0;
	}
	//Pad the message
	while(_protocolPacketBufferPosition < MINIMUM_MESSAGE_SIZE)
	{
		_protocolPacketBuffer[_protocolPacketBufferPosition++] = 0x00;
	}
	//Add a CRC32
	CRC32 crc;
	crc.add((uint8_t*)_protocolPacketBuffer, _protocolPacketBufferPosition);
	_protocolPacketBuffer[_protocolPacketBufferPosition++] = (crc.calc() & 0xff000000) >> 24; //CRC
	_protocolPacketBuffer[_protocolPacketBufferPosition++] = (crc.calc() & 0x00ff0000) >> 16;
	_protocolPacketBuffer[_protocolPacketBufferPosition++] = (crc.calc() & 0x0000ff00) >> 8;
	_protocolPacketBuffer[_protocolPacketBufferPosition++] = (crc.calc() & 0x000000ff);
	//Debug info
	if(debug_uart_ != nullptr)
	{
		if(_encyptionEnabled == true)
		{
			debug_uart_->printf_P(PSTR("\n\rCreated pairing message with channel:%d\r\n\tlocal MAC address:%02x%02x%02x%02x%02x%02x\r\n\tGlobal encryption key:%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\r\n\tLocal encryption key:%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\r\n\tCRC:%02x%02x%02x%02x"),
				_communicationChannel,
				_localMacAddress[0],
				_localMacAddress[1],
				_localMacAddress[2],
				_localMacAddress[3],
				_localMacAddress[4],
				_localMacAddress[5],
				_primaryEncryptionKey[0],
				_primaryEncryptionKey[1],
				_primaryEncryptionKey[2],
				_primaryEncryptionKey[3],
				_primaryEncryptionKey[4],
				_primaryEncryptionKey[5],
				_primaryEncryptionKey[6],
				_primaryEncryptionKey[7],
				_primaryEncryptionKey[8],
				_primaryEncryptionKey[9],
				_primaryEncryptionKey[10],
				_primaryEncryptionKey[11],
				_primaryEncryptionKey[12],
				_primaryEncryptionKey[13],
				_primaryEncryptionKey[14],
				_primaryEncryptionKey[15],
				_localEncryptionKey[0],
				_localEncryptionKey[1],
				_localEncryptionKey[2],
				_localEncryptionKey[3],
				_localEncryptionKey[4],
				_localEncryptionKey[5],
				_localEncryptionKey[6],
				_localEncryptionKey[7],
				_localEncryptionKey[8],
				_localEncryptionKey[9],
				_localEncryptionKey[10],
				_localEncryptionKey[11],
				_localEncryptionKey[12],
				_localEncryptionKey[13],
				_localEncryptionKey[14],
				_localEncryptionKey[15],
				_protocolPacketBuffer[_protocolPacketBufferPosition - 4],
				_protocolPacketBuffer[_protocolPacketBufferPosition - 3],
				_protocolPacketBuffer[_protocolPacketBufferPosition - 2],
				_protocolPacketBuffer[_protocolPacketBufferPosition - 1]
				);
		} else {
			debug_uart_->printf_P(PSTR("\n\rCreated pairing message with channel:%d local MAC address:%02x%02x%02x%02x%02x%02x, CRC:%02x%02x%02x%02x"),
				_communicationChannel,
				_localMacAddress[0],
				_localMacAddress[1],
				_localMacAddress[2],
				_localMacAddress[3],
				_localMacAddress[4],
				_localMacAddress[5],
				_protocolPacketBuffer[_protocolPacketBufferPosition - 4],
				_protocolPacketBuffer[_protocolPacketBufferPosition - 3],
				_protocolPacketBuffer[_protocolPacketBufferPosition - 2],
				_protocolPacketBuffer[_protocolPacketBufferPosition - 1]
				);
		}
		if(localDeviceName != nullptr)
		{
			debug_uart_->printf_P(PSTR("\n\r\tName: %s"), localDeviceName);
		}
	}
}
/*
 *
 *	This method builds the pairing ACK message
 *
 */
void ICACHE_FLASH_ATTR m2mDirectClass::_createPairingAckMessage()
{
	_protocolPacketBufferPosition = 0;
	//Add packet flag
	_protocolPacketBuffer[_protocolPacketBufferPosition++] = M2M_DIRECT_PAIRING_ACK_FLAG;
	//Add expected communication channel
	_protocolPacketBuffer[_protocolPacketBufferPosition++] = _communicationChannel;
	//Add local MAC address
	memcpy(&_protocolPacketBuffer[_protocolPacketBufferPosition], _localMacAddress, MAC_ADDRESS_LENGTH);
	_protocolPacketBufferPosition+=MAC_ADDRESS_LENGTH;
	//Add remote MAC address
	memcpy(&_protocolPacketBuffer[_protocolPacketBufferPosition], _remoteMacAddress, MAC_ADDRESS_LENGTH);
	_protocolPacketBufferPosition+=MAC_ADDRESS_LENGTH;
	//Add global encryption key
	memcpy(&_protocolPacketBuffer[_protocolPacketBufferPosition], _primaryEncryptionKey, ENCRYPTION_KEY_LENGTH);
	_protocolPacketBufferPosition+=ENCRYPTION_KEY_LENGTH;
	//Add local encryption key
	memcpy(&_protocolPacketBuffer[_protocolPacketBufferPosition], _localEncryptionKey, ENCRYPTION_KEY_LENGTH);
	_protocolPacketBufferPosition+=ENCRYPTION_KEY_LENGTH;
	//Add local name if there is one
	if(localDeviceName != nullptr)
	{
		//Add the name length
		_protocolPacketBuffer[_protocolPacketBufferPosition++] = strlen(localDeviceName);
		//Add the name
		memcpy(&_protocolPacketBuffer[_protocolPacketBufferPosition], localDeviceName, strlen(localDeviceName));
		_protocolPacketBufferPosition+=strlen(localDeviceName);
	}
	else
	{
		//Add the name length of zero to signify no name
		_protocolPacketBuffer[_protocolPacketBufferPosition++] = 0;
	}
	//Pad the message
	while(_protocolPacketBufferPosition < MINIMUM_MESSAGE_SIZE)
	{
		_protocolPacketBuffer[_protocolPacketBufferPosition++] = 0x00;
	}
	//Add a CRC32
	CRC32 crc;
	crc.add((uint8_t*)_protocolPacketBuffer, _protocolPacketBufferPosition);
	_protocolPacketBuffer[_protocolPacketBufferPosition++] = (crc.calc() & 0xff000000) >> 24; //CRC
	_protocolPacketBuffer[_protocolPacketBufferPosition++] = (crc.calc() & 0x00ff0000) >> 16;
	_protocolPacketBuffer[_protocolPacketBufferPosition++] = (crc.calc() & 0x0000ff00) >> 8;
	_protocolPacketBuffer[_protocolPacketBufferPosition++] = (crc.calc() & 0x000000ff);
	//Debug info
	if(debug_uart_ != nullptr)
	{
		debug_uart_->printf_P(PSTR("\n\rCreated pairing ACK message with channel:%d\r\n\tLocal  MAC address:%02x%02x%02x%02x%02x%02x\r\n\tRemote MAC address:%02x%02x%02x%02x%02x%02x\r\n\tGlobal encryption key:%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\r\n\tLocal encryption key:%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\r\n\tCRC:%02x%02x%02x%02x"),
			_protocolPacketBuffer[1],//Channel`
			_protocolPacketBuffer[2],//Local MAC
			_protocolPacketBuffer[3],
			_protocolPacketBuffer[4],
			_protocolPacketBuffer[5],
			_protocolPacketBuffer[6],
			_protocolPacketBuffer[7],
			_protocolPacketBuffer[8],//Remote MAC
			_protocolPacketBuffer[9],
			_protocolPacketBuffer[10],
			_protocolPacketBuffer[11],
			_protocolPacketBuffer[12],
			_protocolPacketBuffer[13],
			_protocolPacketBuffer[14],//Global key
			_protocolPacketBuffer[15],
			_protocolPacketBuffer[16],
			_protocolPacketBuffer[17],
			_protocolPacketBuffer[18],
			_protocolPacketBuffer[19],
			_protocolPacketBuffer[20],
			_protocolPacketBuffer[21],
			_protocolPacketBuffer[22],
			_protocolPacketBuffer[23],
			_protocolPacketBuffer[24],
			_protocolPacketBuffer[25],
			_protocolPacketBuffer[26],
			_protocolPacketBuffer[27],
			_protocolPacketBuffer[28],
			_protocolPacketBuffer[29],
			_protocolPacketBuffer[30],//Local key
			_protocolPacketBuffer[31],
			_protocolPacketBuffer[32],
			_protocolPacketBuffer[33],
			_protocolPacketBuffer[34],
			_protocolPacketBuffer[35],
			_protocolPacketBuffer[36],
			_protocolPacketBuffer[37],
			_protocolPacketBuffer[38],
			_protocolPacketBuffer[39],
			_protocolPacketBuffer[40],
			_protocolPacketBuffer[41],
			_protocolPacketBuffer[42],
			_protocolPacketBuffer[43],
			_protocolPacketBuffer[44],
			_protocolPacketBuffer[45],
			_protocolPacketBuffer[_protocolPacketBufferPosition - 4],
			_protocolPacketBuffer[_protocolPacketBufferPosition - 3],
			_protocolPacketBuffer[_protocolPacketBufferPosition - 2],
			_protocolPacketBuffer[_protocolPacketBufferPosition - 1]
			);
		if(localDeviceName != nullptr)
		{
			debug_uart_->printf_P(PSTR("\n\r\tName: %s"), localDeviceName);
		}
	}
}
/*
 *
 *	This method builds the keepalive message
 *
 */
void ICACHE_FLASH_ATTR m2mDirectClass::_createKeepaliveMessage()
{
	_protocolPacketBufferPosition = 0;
	//Add packet flag
	_protocolPacketBuffer[_protocolPacketBufferPosition++] = M2M_DIRECT_KEEPALIVE_FLAG;
	//Add expected communication channel
	_protocolPacketBuffer[_protocolPacketBufferPosition++] = _communicationChannel;
	//Add local MAC address
	memcpy(&_protocolPacketBuffer[_protocolPacketBufferPosition], _localMacAddress, MAC_ADDRESS_LENGTH);
	_protocolPacketBufferPosition+=MAC_ADDRESS_LENGTH;
	//Add remote MAC address
	memcpy(&_protocolPacketBuffer[_protocolPacketBufferPosition], _remoteMacAddress, MAC_ADDRESS_LENGTH);
	_protocolPacketBufferPosition+=MAC_ADDRESS_LENGTH;
	//Add local timestamp
	_protocolPacketBuffer[_protocolPacketBufferPosition++] = (_localActivityTimer & 0xff000000) >> 24;
	_protocolPacketBuffer[_protocolPacketBufferPosition++] = (_localActivityTimer & 0x00ff0000) >> 16;
	_protocolPacketBuffer[_protocolPacketBufferPosition++] = (_localActivityTimer & 0x0000ff00) >> 8;
	_protocolPacketBuffer[_protocolPacketBufferPosition++] = (_localActivityTimer & 0x000000ff);
	//Add last remote timestamp
	_protocolPacketBuffer[_protocolPacketBufferPosition++] = (_remoteActivityTimer & 0xff000000) >> 24;
	_protocolPacketBuffer[_protocolPacketBufferPosition++] = (_remoteActivityTimer & 0x00ff0000) >> 16;
	_protocolPacketBuffer[_protocolPacketBufferPosition++] = (_remoteActivityTimer & 0x0000ff00) >> 8;
	_protocolPacketBuffer[_protocolPacketBufferPosition++] = (_remoteActivityTimer & 0x000000ff);
	_protocolPacketBuffer[_protocolPacketBufferPosition++] = _minTxPower;
	_protocolPacketBuffer[_protocolPacketBufferPosition++] = _currentTxPower;
	_protocolPacketBuffer[_protocolPacketBufferPosition++] = _maxTxPower;
	//Pad the message
	while(_protocolPacketBufferPosition < MINIMUM_MESSAGE_SIZE)
	{
		_protocolPacketBuffer[_protocolPacketBufferPosition++] = 0x00;
	}
	//Add a CRC32
	CRC32 crc;
	crc.add((uint8_t*)_protocolPacketBuffer, _protocolPacketBufferPosition);
	_protocolPacketBuffer[_protocolPacketBufferPosition++] = (crc.calc() & 0xff000000) >> 24; //CRC
	_protocolPacketBuffer[_protocolPacketBufferPosition++] = (crc.calc() & 0x00ff0000) >> 16;
	_protocolPacketBuffer[_protocolPacketBufferPosition++] = (crc.calc() & 0x0000ff00) >> 8;
	_protocolPacketBuffer[_protocolPacketBufferPosition++] = (crc.calc() & 0x000000ff);
	
}

/*
 *
 *	This method sends broadcast ESP-Now messages, mostly used for pairing
 *
 */
bool ICACHE_FLASH_ATTR m2mDirectClass::_sendBroadcastPacket(uint8_t* buffer, uint8_t length)
{
	if(debug_uart_ != nullptr)
	{
		debug_uart_->printf_P(PSTR("\n\rTX %03u bytes broadcast on channel:%d %.2fdBm "), length, _currentChannel(), (float)_currentTxPower * 0.25);
		_printPacketDescription(_protocolPacketBuffer[0]);
	}
	int result = esp_now_send(_broadcastMacAddress, buffer, length);
	if(result == ESP_OK)
	{
		if(debug_uart_ != nullptr)
		{
			debug_uart_->print(F(" OK"));
		}
		return true;
	}
	else
	{
		if(debug_uart_ != nullptr)
		{
			debug_uart_->print(F(" failed"));
		}
	}
	return false;
}
/*
 *
 *	This method sends unicast messages, used for the connection once paired
 *
 */
bool ICACHE_FLASH_ATTR m2mDirectClass::_sendUnicastPacket(uint8_t* buffer, uint8_t length, bool wait)
{
	if(esp_now_is_peer_exist(_remoteMacAddress) == 0)
	{
		if(_encyptionEnabled == true)
		{
			_registerPeer(_remoteMacAddress, _communicationChannel, _localEncryptionKey);
		}
		else
		{
			_registerPeer(_remoteMacAddress, _communicationChannel);
		}
	}
	#ifdef M2M_DIRECT_DEBUG_SEND
	if(debug_uart_ != nullptr)
	{
		debug_uart_->printf_P(PSTR("\n\rTX %03u bytes   to:%02x%02x%02x%02x%02x%02x "), length, _remoteMacAddress[0], _remoteMacAddress[1], _remoteMacAddress[2], _remoteMacAddress[3], _remoteMacAddress[4], _remoteMacAddress[5]);
		_printPacketDescription(buffer[0]);
	}
	#endif
	_waitingForSendCallback = wait;
	_sendQuality = _sendQuality >> 1;	//Reduce signal quality
	uint32_t packetSent = millis();
	int result = esp_now_send(_remoteMacAddress, buffer, length);
	if(result == ESP_OK)
	{
        while(_waitingForSendCallback && millis() - packetSent < _sendTimeout)
        {
          yield();
        }
		if(_waitingForSendCallback == false)
		{
			_sendQuality = _sendQuality | 0x80000000;  //Improve signal quality, MSB first
			#ifdef M2M_DIRECT_DEBUG_SEND
			if(debug_uart_ != nullptr)
			{
				debug_uart_->printf(" sendQ:%08x echoQ:%08x %.2fdBm", _sendQuality, _echoQuality, (float)_currentTxPower * 0.25);
			}
			#endif
			if(_sendQuality == 0xffffffff)
			{
				_increaseKeepaliveInterval();
			}
			return true;
		}
		else
		{
			#ifdef M2M_DIRECT_DEBUG_SEND
			if(debug_uart_ != nullptr)
			{
				debug_uart_->printf(" timeout sendQ:%08x echoQ:%08x %.2fdBm", _sendQuality, _echoQuality, (float)_currentTxPower * 0.25);
			}
			#endif
			_waitingForSendCallback = false;
		}
	}
	else
	{
		#ifdef M2M_DIRECT_DEBUG_SEND
		if(debug_uart_ != nullptr)
		{
			debug_uart_->printf(" failed sendQ:%08x echoQ:%08x %.2fdBm", _sendQuality, _echoQuality, (float)_currentTxPower * 0.25);
		}
		#endif
	}
	_decreaseKeepaliveInterval();
	return false;
}
/*
 *
 *	Returns the number of 1 bits in an uint32_t, used in measuring link quality
 *
 */
uint8_t ICACHE_FLASH_ATTR m2mDirectClass::_countBits(uint32_t thingToCount)
{
  uint8_t result = 0;
  for(uint8_t i = 0; i < 32 ; i++)
  {
    result+=(0x00000001 << i) & thingToCount ? 1 : 0;
  }
  return result;
}
void ICACHE_FLASH_ATTR m2mDirectClass::disableEncryption()
{
	_encyptionEnabled = false;
}

void ICACHE_FLASH_ATTR m2mDirectClass::_printPacketDescription(uint8_t type)
{
	if(debug_uart_ != nullptr)
	{
		if(type == M2M_DIRECT_PAIRING_FLAG)
		{
			debug_uart_->print(F("PAIRING    "));
		}
		else if(type == M2M_DIRECT_PAIRING_ACK_FLAG)
		{
			debug_uart_->print(F("PAIRING ACK"));
		}
		else if(type == M2M_DIRECT_KEEPALIVE_FLAG)
		{
			debug_uart_->print(F("KEEPALIVE  "));
		}
		else if(type == M2M_DIRECT_DATA_FLAG)
		{
			debug_uart_->print(F("APPLICATION"));
		}
	}
}
void ICACHE_FLASH_ATTR m2mDirectClass::_debugState()
{
	debug_uart_->print(F("\n\rState: "));
	_printCurrentState();
	if(remoteDeviceName != nullptr)
	{
		if(state == m2mDirectState::paired)
		{
			debug_uart_->print(F(" with "));
			debug_uart_->print(remoteDeviceName);
		}
		else if(state == m2mDirectState::connecting || state == m2mDirectState::connected)
		{
			debug_uart_->print(F(" to "));
			debug_uart_->print(remoteDeviceName);
		}
		else if(state == m2mDirectState::disconnected)
		{
			debug_uart_->print(F(" from "));
			debug_uart_->print(remoteDeviceName);
		}
	}
}
void ICACHE_FLASH_ATTR m2mDirectClass::_printCurrentState()
{
	if(debug_uart_ != nullptr)
	{
		if(state == m2mDirectState::uninitialised)
		{
			debug_uart_->print(F("uninitialised"));
		}
		else if(state == m2mDirectState::initialised)
		{
			debug_uart_->print(F("initialised"));
		}
		else if(state == m2mDirectState::started)
		{
			debug_uart_->print(F("started"));
		}
		else if(state == m2mDirectState::scanning)
		{
			debug_uart_->print(F("scanning"));
		}
		else if(state == m2mDirectState::pairing)
		{
			debug_uart_->print(F("pairing"));
		}
		else if(state == m2mDirectState::paired)
		{
			debug_uart_->print(F("paired"));
		}
		else if(state == m2mDirectState::connecting)
		{
			debug_uart_->print(F("connecting"));
		}
		else if(state == m2mDirectState::connected)
		{
			debug_uart_->print(F("connected"));
		}
		else if(state == m2mDirectState::disconnected)
		{
			debug_uart_->print(F("disconnected"));
		}
	}
}
/*
 *
 *	Sets the callback function for when the device starts 'pairing'
 *
 */
m2mDirectClass& m2mDirectClass::setPairingCallback(std::function<void()> function) {
    this->pairingCallback = function;
    return *this;
}
/*
 *
 *	Sets the callback function for when the device is 'paired'
 *
 */
m2mDirectClass& m2mDirectClass::setPairedCallback(std::function<void()> function) {
    this->pairedCallback = function;
    return *this;
}
/*
 *
 *	Sets the callback function for when the device is 'conected'
 *
 */
m2mDirectClass& m2mDirectClass::setConnectedCallback(std::function<void()> function) {
    this->connectedCallback = function;
    return *this;
}
/*
 *
 *	Sets the callback function for when the device is 'disconnected'
 *
 */
m2mDirectClass& m2mDirectClass::setDisconnectedCallback(std::function<void()> function) {
    this->disconnectedCallback = function;
    return *this;
}
/*
 *
 *	Sets the callback function for when the device receives a message
 *
 */
m2mDirectClass& m2mDirectClass::setMessageReceivedCallback(std::function<void()> function) {
    this->messageReceivedCallback = function;
    return *this;
}
/*
 *
 *	This returns the link quality heuristic. Higher is better
 *
 */
uint32_t ICACHE_FLASH_ATTR m2mDirectClass::linkQuality()
{
	return _sendQuality & _echoQuality;
}
/*
 *
 *	Simple boolean measure of being connected
 *
 */
bool ICACHE_FLASH_ATTR m2mDirectClass::connected()
{
	return state == m2mDirectState::connected;
}
/*
 *
 *	Sends the message, after data has been added. Parameter 'wait' is used to specify if it should wait for confirmation
 *	It should be noted confirmation is not guarantee of delivery, but failure is guarantee of failure
 *
 */
bool ICACHE_FLASH_ATTR m2mDirectClass::sendMessage(bool wait)
{
	CRC32 crc;
	_applicationPacketBuffer[0] = M2M_DIRECT_DATA_FLAG;	//Make sure this is set as a data packet
	while(_applicationBufferPosition < MINIMUM_MESSAGE_SIZE)
	{
		_applicationPacketBuffer[_applicationBufferPosition++] = 0xff;
	}
	crc.add((uint8_t*)_applicationPacketBuffer, _applicationBufferPosition);	//Calculate the CRC
	_applicationPacketBuffer[_applicationBufferPosition++] = (crc.calc() & 0xff000000) >> 24;	//Add the CRC
	_applicationPacketBuffer[_applicationBufferPosition++] = (crc.calc() & 0x00ff0000) >> 16;
	_applicationPacketBuffer[_applicationBufferPosition++] = (crc.calc() & 0x0000ff00) >> 8;
	_applicationPacketBuffer[_applicationBufferPosition++] = (crc.calc() & 0x000000ff);
	if(state == m2mDirectState::connected && _sendUnicastPacket(_applicationPacketBuffer, _applicationBufferPosition, wait))
	{
		_applicationBufferPosition = 2;		//Reset the buffer position for the next message
		_applicationPacketBuffer[1] = 0;	//Reset the field count for the next message
		//_advanceTimers();	//Advance the timers for keepalives
		return true;
	}
	else
	{
		_applicationBufferPosition = 2;		//Reset the buffer position for the next message
		_applicationPacketBuffer[1] = 0;	//Reset the field count for the next message
	}
	//_advanceTimers();	//Advance the timers for keepalives
	return false;
}
/*
 *
 *	Clears any received message so another can be received. There is only one incoming packet buffer!
 *
 */
void ICACHE_FLASH_ATTR m2mDirectClass::clearReceivedMessage()
{
	_receivedPacketBuffer[1] = 0;		//Reset the field count for the next message
	_receivedPacketBufferPosition = 2;	//Reset the buffer position for the next message
	if(m2m.debug_uart_ != nullptr)
	{
		m2m.debug_uart_->print(F("\n\rReceived message cleared"));
	}
}
/*
 *
 *	Returns the current 2.4Ghz channel
 *
 */
uint8_t ICACHE_FLASH_ATTR m2mDirectClass::_currentChannel()
{
	#if defined(ESP8266)
	uint8_t currentChannel = wifi_get_channel();
	#elif defined ESP32
	//uint8_t currentChannel = 0;
	//uint8_t secondaryChannel = 0;
	//esp_wifi_get_channel(&currentChannel, &secondaryChannel);
	uint8_t currentChannel = WiFi.channel();
	#endif
	return currentChannel;
}
/*
 *
 *	Returns the number of fields are there left in the message
 *
 */
uint8_t ICACHE_FLASH_ATTR m2mDirectClass::dataAvailable()
{
	return _receivedPacketBuffer[1];
}
/*
 *
 *	Returns the 'type' of the next field
 *
 */
uint8_t ICACHE_FLASH_ATTR m2mDirectClass::nextDataType()
{
	if(_receivedPacketBuffer[1] == 0)
	{
		return DATA_UNAVAILABLE;
	}
	if(_receivedPacketBuffer[_receivedPacketBufferPosition] == DATA_BOOL_TRUE)	//Handles the case of a single bool true
	{
		return DATA_BOOL;
	}
	return (_receivedPacketBuffer[_receivedPacketBufferPosition] & 0x8f);	//Strip out any array size before returning it
}
/*
 *
 *	Returns the 'length' of the next field for strings and arrays
 *
 */
uint8_t ICACHE_FLASH_ATTR m2mDirectClass::nextDataLength()
{
	if(_receivedPacketBuffer[1] == 0 || ((_receivedPacketBuffer[_receivedPacketBufferPosition] & 0x80) == 0 && _receivedPacketBuffer[_receivedPacketBufferPosition] != 0x0d))
	{
		return 0;
	}
	return _receivedPacketBuffer[_receivedPacketBufferPosition+1];
}
/*
 *
 *	Skip the next field
 *
 */
void ICACHE_FLASH_ATTR m2mDirectClass::skipReceivedData()
{
	if(_receivedPacketBuffer[1] == 0)
	{
		return;
	}
	_receivedPacketBuffer[1]--;	//Mark the field as retrieved
	if(_receivedPacketBuffer[1] == 0)
	{
		_receivedPacketBufferPosition = 2;		//Reset the buffer position for the next message
	}
	else
	{
		switch (_receivedPacketBuffer[_receivedPacketBufferPosition] & 0x0f)
		{
			case DATA_BOOL:
				_receivedPacketBufferPosition++;
			break;
			case DATA_BOOL_TRUE:
				_receivedPacketBufferPosition++;
			break;
			case DATA_UINT8_T:
				_receivedPacketBufferPosition+=sizeof(uint8_t)+1;
			break;
			case DATA_UINT16_T:
				_receivedPacketBufferPosition+=sizeof(uint16_t)+1;
			break;
			case DATA_UINT32_T:
				_receivedPacketBufferPosition+=sizeof(uint32_t)+1;
			break;
			case DATA_UINT64_T:
				_receivedPacketBufferPosition+=sizeof(uint64_t)+1;
			break;
			case DATA_INT8_T:
				_receivedPacketBufferPosition+=sizeof(int8_t)+1;
			break;
			case DATA_INT16_T:
				_receivedPacketBufferPosition+=sizeof(int16_t)+1;
			break;
			case DATA_INT32_T:
				_receivedPacketBufferPosition+=sizeof(int32_t)+1;
			break;
			case DATA_INT64_T:
				_receivedPacketBufferPosition+=sizeof(int64_t)+1;
			break;
			case DATA_FLOAT:
				_receivedPacketBufferPosition+=sizeof(float)+1;
			break;
			case DATA_DOUBLE:
				_receivedPacketBufferPosition+=sizeof(double)+1;
			break;
			case DATA_CHAR:
				_receivedPacketBufferPosition+=sizeof(char)+1;
			break;
			case DATA_STR:
				//if((_receivedPacketBuffer[_receivedPacketBufferPosition] & 0xf0) == 0)	//Medium char array, length in next byte
				{
					_receivedPacketBufferPosition+=_receivedPacketBuffer[_receivedPacketBufferPosition+1]+1;
				}
				/*
				else if((_receivedPacketBuffer[_receivedPacketBufferPosition] & 0xf0) < M2M_DIRECT_SMALL_ARRAY_LIMIT + 1)	//Short char array, length in type field
				{
					_receivedPacketBufferPosition+=((_receivedPacketBuffer[_receivedPacketBufferPosition] & 0xf0) >> 4);	//Allows for zero length strings
				}
				*/
			break;
			case DATA_CUSTOM:
				_receivedPacketBufferPosition+=_receivedPacketBuffer[_receivedPacketBufferPosition];
			break;
		}
	}
}
/*
 *
 *	Prints the 'type' for a field label in debugging
 *
 */
void ICACHE_FLASH_ATTR m2mDirectClass::_dataTypeDescription(uint8_t type)
{
	if(type == DATA_UNAVAILABLE)
	{
		debug_uart_->print(F("UNAVAILABLE"));
	}
	else if(type == DATA_BOOL || type == DATA_BOOL_TRUE)
	{
		debug_uart_->print(F("BOOL"));
	}
	else if(type == DATA_UINT8_T)
	{
		debug_uart_->print(F("UINT8_T"));
	}
	else if(type == DATA_UINT16_T)
	{
		debug_uart_->print(F("UINT16_T"));
	}
	else if(type == DATA_UINT32_T)
	{
		debug_uart_->print(F("UINT32_T"));
	}
	else if(type == DATA_UINT64_T)
	{
		debug_uart_->print(F("UINT64_T"));
	}
	else if(type == DATA_INT8_T)
	{
		debug_uart_->print(F("INT8_t"));
	}
	else if(type == DATA_INT16_T)
	{
		debug_uart_->print(F("INT16_T"));
	}
	else if(type == DATA_INT32_T)
	{
		debug_uart_->print(F("INT32_T"));
	}
	else if(type == DATA_INT64_T)
	{
		debug_uart_->print(F("INT64_t"));
	}
	else if(type == DATA_FLOAT)
	{
		debug_uart_->print(F("FLOAT"));
	}
	else if(type == DATA_DOUBLE)
	{
		debug_uart_->print(F("DOUBLE"));
	}
	else if(type == DATA_CHAR)
	{
		debug_uart_->print(F("CHAR"));
	}
	else if(type == DATA_BOOL_ARRAY)
	{
		debug_uart_->print(F("BOOL_ARRAY"));
	}
	else if(type == DATA_UINT8_T_ARRAY)
	{
		debug_uart_->print(F("UINT8_T_ARRAY"));
	}
	else if(type == DATA_UINT16_T_ARRAY)
	{
		debug_uart_->print(F("UINT16_T_ARRAY"));
	}
	else if(type == DATA_UINT32_T_ARRAY)
	{
		debug_uart_->print(F("UINT32_T_ARRAY"));
	}
	else if(type == DATA_UINT64_T_ARRAY)
	{
		debug_uart_->print(F("UINT64_T_ARRAY"));
	}
	else if(type == DATA_INT8_T_ARRAY)
	{
		debug_uart_->print(F("INT8_t_ARRAY"));
	}
	else if(type == DATA_INT16_T_ARRAY)
	{
		debug_uart_->print(F("INT16_T_ARRAY"));
	}
	else if(type == DATA_INT32_T_ARRAY)
	{
		debug_uart_->print(F("INT32_T_ARRAY"));
	}
	else if(type == DATA_INT64_T_ARRAY)
	{
		debug_uart_->print(F("INT64_t_ARRAY"));
	}
	else if(type == DATA_FLOAT_ARRAY)
	{
		debug_uart_->print(F("FLOAT_ARRAY"));
	}
	else if(type == DATA_DOUBLE_ARRAY)
	{
		debug_uart_->print(F("DOUBLE_ARRAY"));
	}
	else if(type == DATA_CHAR_ARRAY)
	{
		debug_uart_->print(F("CHAR_ARRAY"));
	}
	else if(type == DATA_STR)
	{
		debug_uart_->print(F("C string"));
	}
	else
	{
		debug_uart_->print(F("UNKNOWN"));
	}
	debug_uart_->printf_P(PSTR("(%02x)"), type);
}
/*
	debug_uart_->print(F("\r\nAdding "));
	switch (dataType)
	{
	  case DATA_UINT8_T:
		debug_uart_->print(F("uint8_t"));
	  break;
	  case DATA_UINT16_T:
		debug_uart_->print(F("uint16_t"));
	  break;
	  case DATA_UINT32_T:
		debug_uart_->print(F("uint32_t"));
	  break;
	  case DATA_UINT64_T:
		debug_uart_->print(F("uint64_t"));
	  break;
	  case DATA_INT8_T:
		debug_uart_->print(F("int8_t"));
	  break;
	  case DATA_INT16_T:
		debug_uart_->print(F("int16_t"));
	  break;
	  case DATA_INT32_T:
		debug_uart_->print(F("int32_t"));
	  break;
	  case DATA_FLOAT:
		debug_uart_->print(F("float"));
	  break;
	  case DATA_DOUBLE:
		debug_uart_->print(F("double"));
	  break;
	  case DATA_CHAR:
		debug_uart_->print(F("char"));
	  break;
	  case DATA_STR:
		debug_uart_->print(F("str"));
	  break;
	  case DATA_CUSTOM:
		debug_uart_->print(F("custom"));
	  break;
	  case DATA_UINT8_T_ARRAY:
		debug_uart_->print(F("uint8_t["));
	  break;
	  case DATA_UINT16_T_ARRAY:
		debug_uart_->print(F("uint16_t["));
	  break;
	  case DATA_UINT32_T_ARRAY:
		debug_uart_->print(F("uint32_t["));
	  break;
	  case DATA_UINT64_T_ARRAY:
		debug_uart_->print(F("uint64_t["));
	  break;
	  case DATA_INT8_T_ARRAY:
		debug_uart_->print(F("int8_t["));
	  break;
	  case DATA_INT16_T_ARRAY:
		debug_uart_->print(F("int16_t["));
	  break;
	  case DATA_INT32_T_ARRAY:
		debug_uart_->print(F("int32_t["));
	  break;
	  case DATA_FLOAT_ARRAY:
		debug_uart_->print(F("float["));
	  break;
	  case DATA_DOUBLE_ARRAY:
		debug_uart_->print(F("double["));
	  break;
	  case DATA_CHAR_ARRAY:
		debug_uart_->print(F("char["));
	  break;
	  case DATA_CUSTOM_ARRAY:
		debug_uart_->print(F("custom["));
	  break;
	}
*/
/*
 *
 *	Compares two MAC addresses and returns true if the first is higher
 *
 */
bool ICACHE_FLASH_ATTR m2mDirectClass::_tieBreak(uint8_t* macAddress1, uint8_t* macAddress2)
{
	if(memcmp(macAddress1,macAddress2,MAC_ADDRESS_LENGTH) > 0)
	{
		return true;
	}
	return false;
}
bool ICACHE_FLASH_ATTR m2mDirectClass::_remoteMacAddressSet()
{
	if(_remoteMacAddress[0] == 0 &&
		_remoteMacAddress[1] == 0 &&
		_remoteMacAddress[2] == 0 &&
		_remoteMacAddress[3] == 0 &&
		_remoteMacAddress[4] == 0 &&
		_remoteMacAddress[5] == 0
	)
	{
		return false;
	}
	return true;
}
/*
 *
 *	Read pairing from EEPROM (ESP8266) or 'preferences' (ESP32)
 *
 */
bool ICACHE_FLASH_ATTR m2mDirectClass::_readPairingInfo()
{
	#if defined(ESP8266)
		if(m2m.debug_uart_ != nullptr)
		{
			m2m.debug_uart_->print(F("\n\rReading pairing info from EEPROM: "));
		}
		uint8_t eepromData[EEPROM_DATA_SIZE];
		for(uint8_t address = 0; address < EEPROM_DATA_SIZE; address++)
		{
			eepromData[address] = EEPROM.read(address);
		}
		CRC32 crc;
		crc.add(eepromData, EEPROM_DATA_SIZE - 4);
		uint32_t crcFromEEPROM = eepromData[41];
		crcFromEEPROM+=uint32_t(eepromData[40]) << 8;
		crcFromEEPROM+=uint32_t(eepromData[39]) << 16;
		crcFromEEPROM+=uint32_t(eepromData[38]) << 24;
		if(crc.calc() == crcFromEEPROM)
		{
			if(m2m.debug_uart_ != nullptr)
			{
				m2m.debug_uart_->print(F("OK CRC: "));
				m2m.debug_uart_->print(crcFromEEPROM);
			}
			for(uint8_t address = 0; address < 6; address++)
			{
				_remoteMacAddress[address] = eepromData[address];
			}
			for(uint8_t address = 6; address < 22; address++)
			{
				_primaryEncryptionKey[address - 6] = eepromData[address];
			}
			for(uint8_t address = 22; address < 38; address++)
			{
				 _localEncryptionKey[address - 22] = eepromData[address];
			}
			if(m2m.debug_uart_ != nullptr)
			{
				Serial.printf_P(PSTR("OK\r\n\tMAC address:%02x%02x%02x%02x%02x%02x\r\n\tPrimary encryption key:%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\r\n\tLocal encryption key: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"), _remoteMacAddress, _primaryEncryptionKey, _localEncryptionKey);
			}
			return true;
		}
		else
		{
			if(m2m.debug_uart_ != nullptr)
			{
				m2m.debug_uart_->print(F("invalid CRC32"));
			}
		}
	#elif defined ESP32
		if(m2m.debug_uart_ != nullptr)
		{
			m2m.debug_uart_->print(F("\n\rReading pairing info from Preferences: "));
		}
		uint8_t successes = 0;
		settings.begin(preferencesNamespace, false);
		successes+=settings.getBytes(pairedMacKey, _remoteMacAddress, 6);
		successes+=settings.getBytes(pairedPrimaryKey, _primaryEncryptionKey, 16);
		successes+=settings.getBytes(pairedLocalKey, _localEncryptionKey, 16);
		if(settings.getType(pairedNameKey) != PT_INVALID && settings.getType(pairedNameLengthKey) != PT_INVALID)	//There is a name stored
		{
			uint8_t len = settings.getUChar(pairedNameLengthKey, 0);
			if(len > 0)
			{
				remoteDeviceName = new char[len + 1];
				successes+=settings.getString(pairedNameKey, remoteDeviceName, len + 1);
				//remoteDeviceName[len] = 0;
			}
		}
		settings.end();
		if(successes >= 38)
		{
			if(m2m.debug_uart_ != nullptr)
			{
				Serial.printf_P(PSTR("OK\r\n\tMAC address:%02x%02x%02x%02x%02x%02x\r\n\tPrimary encryption key:%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\r\n\tLocal encryption key: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"),
				_remoteMacAddress[0],
				_remoteMacAddress[1],
				_remoteMacAddress[2],
				_remoteMacAddress[3],
				_remoteMacAddress[4],
				_remoteMacAddress[5],
				_primaryEncryptionKey[0],
				_primaryEncryptionKey[1],
				_primaryEncryptionKey[2],
				_primaryEncryptionKey[3],
				_primaryEncryptionKey[4],
				_primaryEncryptionKey[5],
				_primaryEncryptionKey[6],
				_primaryEncryptionKey[7],
				_primaryEncryptionKey[8],
				_primaryEncryptionKey[9],
				_primaryEncryptionKey[10],
				_primaryEncryptionKey[11],
				_primaryEncryptionKey[12],
				_primaryEncryptionKey[13],
				_primaryEncryptionKey[14],
				_primaryEncryptionKey[15],
				_localEncryptionKey[0],
				_localEncryptionKey[1],
				_localEncryptionKey[2],
				_localEncryptionKey[3],
				_localEncryptionKey[4],
				_localEncryptionKey[5],
				_localEncryptionKey[6],
				_localEncryptionKey[7],
				_localEncryptionKey[8],
				_localEncryptionKey[9],
				_localEncryptionKey[10],
				_localEncryptionKey[11],
				_localEncryptionKey[12],
				_localEncryptionKey[13],
				_localEncryptionKey[14],
				_localEncryptionKey[15]
				);
				if(remoteDeviceName != nullptr)
				{
					Serial.print(F("\r\n\tRemote device name:"));
					Serial.print(remoteDeviceName);
				}
			}
			return true;
		}
		if(m2m.debug_uart_ != nullptr)
		{
			m2m.debug_uart_->print(F("failed"));
		}
	#endif
	return false;
}
/*
 *
 *	Write pairing from EEPROM (ESP8266) or 'preferences' (ESP32)
 *
 */
bool ICACHE_FLASH_ATTR m2mDirectClass::_writePairingInfo()
{
	#if defined(ESP8266)
	if(m2m.debug_uart_ != nullptr)
	{
		m2m.debug_uart_->print(F("\n\rWriting pairing info to EEPROM: "));
	}
	uint8_t eepromData[EEPROM_DATA_SIZE];
	for(uint8_t address = 0; address < 6; address++)
	{
		eepromData[address] = _remoteMacAddress[address];
	}
	for(uint8_t address = 6; address < 22; address++)
	{
		eepromData[address] = _primaryEncryptionKey[address - 6];
	}
	for(uint8_t address = 22; address < 38; address++)
	{
		eepromData[address] = _localEncryptionKey[address - 22];
	}
	CRC32 crc;
	crc.add(eepromData, EEPROM_DATA_SIZE - 4);
	eepromData[38] = (crc.calc() & 0xff000000) >> 24; //CRC
	eepromData[39] = (crc.calc() & 0x00ff0000) >> 16;
	eepromData[40] = (crc.calc() & 0x0000ff00) >> 8;
	eepromData[41] = (crc.calc() & 0x000000ff);
	for(uint8_t address = 0; address < EEPROM_DATA_SIZE; address++)
	{
		EEPROM.write(address,eepromData[address]);
	}
	if(EEPROM.commit())
	{
		if(m2m.debug_uart_ != nullptr)
		{
			m2m.debug_uart_->print(F("OK CRC: "));
			m2m.debug_uart_->print(crc.calc());
		}
		return true;
	}
	if(m2m.debug_uart_ != nullptr)
	{
		m2m.debug_uart_->print(F("failed"));
	}
	return false;
	#elif defined ESP32
	if(m2m.debug_uart_ != nullptr)
	{
		m2m.debug_uart_->print(F("\n\rWriting pairing info to Preferences: "));
	}
	uint8_t successes = 0;
	settings.begin(preferencesNamespace, false);
	successes+=settings.putBytes(pairedMacKey, _remoteMacAddress, 6);
	successes+=settings.putBytes(pairedPrimaryKey, _primaryEncryptionKey, 16);
	successes+=settings.putBytes(pairedLocalKey, _localEncryptionKey, 16);
	if(remoteDeviceName != nullptr)
	{
		successes+=settings.putUChar(pairedNameLengthKey, strlen(remoteDeviceName));
		successes+=settings.putString(pairedNameKey, remoteDeviceName);
	}
	settings.end();
	if(successes >= 38)
	{
		if(m2m.debug_uart_ != nullptr)
		{
			Serial.println(F("OK"));
		}
		return true;
	}
	if(m2m.debug_uart_ != nullptr)
	{
		m2m.debug_uart_->print(F("failed"));
	}
	return false;
	#endif
}
/*
 *
 *	Delete pairing from EEPROM (ESP8266) or 'preferences' (ESP32)
 *
 */
bool ICACHE_FLASH_ATTR m2mDirectClass::_deletePairingInfo()
{
	#if defined(ESP8266)
		if(m2m.debug_uart_ != nullptr)
		{
			m2m.debug_uart_->print(F("\n\rDeleting pairing from EEPROM: "));
		}
		for(uint8_t address = 0; address < EEPROM_DATA_SIZE; address++)
		{
			EEPROM.write(address,address);
		}
		if(EEPROM.commit())
		{
			if(m2m.debug_uart_ != nullptr)
			{
				m2m.debug_uart_->print(F("OK"));
			}
			esp_now_del_peer(_remoteMacAddress);
			for(uint8_t address = 0; address < 6; address++)
			{
				_remoteMacAddress[address] = 0;
			}
			for(uint8_t address = 0; address < 16; address++)
			{
				_primaryEncryptionKey[address - 6] = 0;
				_localEncryptionKey[address - 22] = 0;
			}
			if(remoteDeviceName != nullptr)
			{
				delete[] remoteDeviceName;
				remoteDeviceName = nullptr;
			}
			return true;
		}
	#elif defined ESP32
		if(m2m.debug_uart_ != nullptr)
		{
			m2m.debug_uart_->print(F("\n\rDeleting pairing from Preferences: "));
		}
		uint8_t successes = 0;
		settings.begin(preferencesNamespace, false);
		successes+=(settings.remove(pairedMacKey) ? 1 : 0);
		successes+=(settings.remove(pairedPrimaryKey) ? 1 : 0);
		successes+=(settings.remove(pairedLocalKey) ? 1 : 0);
		successes+=(settings.remove(pairedNameLengthKey) ? 1 : 0);
		successes+=(settings.remove(pairedNameKey) ? 1 : 0);
		settings.end();
		successes+=(esp_now_del_peer(_remoteMacAddress) == ESP_OK ? 1 : 0);
		for(uint8_t address = 0; address < 6; address++)
		{
			_remoteMacAddress[address] = 0;
		}
		for(uint8_t address = 0; address < 16; address++)
		{
			_primaryEncryptionKey[address - 6] = 0;
			_localEncryptionKey[address - 22] = 0;
		}
		if(successes >= 4)
		{
			if(m2m.debug_uart_ != nullptr)
			{
				m2m.debug_uart_->print(F("OK"));
			}
		}
		if(remoteDeviceName != nullptr)
		{
			delete[] remoteDeviceName;
			remoteDeviceName = nullptr;
		}
		return true;
	#endif
	if(m2m.debug_uart_ != nullptr)
	{
		m2m.debug_uart_->print(F("failed"));
	}
	return false;
}
/*
 *
 *	Reset pairing info and reset state to re-pair
 *
 */
bool ICACHE_FLASH_ATTR m2mDirectClass::resetPairing()
{
	if(_deletePairingInfo())
	{
		/*
		if(_pairingInfoRead == false)
		{
			if(_encyptionEnabled == true)
			{
				_chooseEncryptionKeys();
			}
			else
			{
				_clearEncryptionKeys();
			}
		}
		*/
		_pairingInfoRead = false;
		_pairingInfoWritten = false;
		if(state == m2mDirectState::connected)
		{
			if(disconnectedCallback != nullptr)
			{
				disconnectedCallback();
			}
		}
		state = m2mDirectState::initialised;
		_indicatorTimerInterval = M2M_DIRECT_INDICATOR_LED_INITIALISED_INTERVAL;
		if(debug_uart_ != nullptr)
		{
			_debugState();
		}
		return true;
	}
	return false;
}
/*
 *
 *	Reduce Tx Power
 *
 */
bool ICACHE_FLASH_ATTR m2mDirectClass::_reduceTxPower()
{
	if(_currentTxPower > _minTxPower)
	{
		if(esp_wifi_set_max_tx_power(_currentTxPower - 1) == ESP_OK)
		{
			if(debug_uart_ != nullptr)
			{
				debug_uart_->printf_P(PSTR("\r\nReduced Tx power to: %.2fdBm"), (float)_currentTxPower * 0.25);
			}
			_currentTxPower--;
			_lastTxPowerChange = millis();
			_lastTxPowerChangeDownwards = true;
			return true;
		}
		else
		{
			if(debug_uart_ != nullptr)
			{
				debug_uart_->print(F("\r\nUnable to reduced Tx power"));
			}
			return false;
		}
	}
	return false;
}
/*
 *
 *	Increase Tx Power
 *
 */
bool ICACHE_FLASH_ATTR m2mDirectClass::_increaseTxPower()
{
	if(_currentTxPower < _maxTxPower)
	{
		if(esp_wifi_set_max_tx_power(_currentTxPower + 1) == ESP_OK)
		{
			if(debug_uart_ != nullptr)
			{
				debug_uart_->printf_P(PSTR("\r\nIncreased Tx power to: %.2fdBm"), (float)_currentTxPower * 0.25);
			}
			_currentTxPower++;
			_lastTxPowerChange = millis();
			_lastTxPowerChangeDownwards = false;
			return true;
		}
		else
		{
			if(debug_uart_ != nullptr)
			{
				debug_uart_->print(F("\r\nUnable to increase Tx power"));
			}
			return false;
		}
	}
	return false;
}
/*
 *
 *	Enable/disable automatic Tx power
 *
 */
void ICACHE_FLASH_ATTR m2mDirectClass::setAutomaticTxPower(bool setting)
{
	_automaticTxPower = setting;
}
m2mDirectClass m2m;	//Create an instance of the class, as only one is practically usable at a time
#endif
