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
#ifndef m2mDirect_h
#define m2mDirect_h
#include <Arduino.h>
//Include the ESP32 WiFi and ESP-Now libraries
#if defined(ESP8266)
	#include "ESP8266WiFi.h"
	extern "C" {
		#include <espnow.h>
		#include <user_interface.h>
	}
	#include <EEPROM.h>
	#define ESP_OK 0
	#define EEPROM_DATA_SIZE 42
#elif defined(ESP32)
	#include <WiFi.h>
	#include <Preferences.h>
	extern "C" {
		#include <esp_now.h>
		#include <esp_wifi.h> // only for esp_wifi_set_channel()
	}
#endif

#ifdef ESP8266
//#include <Ticker.h>
#endif


#include "CRC32.h"

#define MAXIMUM_MESSAGE_SIZE 250	//Note this includes CRC
#define MINIMUM_MESSAGE_SIZE 60	//Note this excludes CRC
#define M2M_DIRECT_PACKET_OVERHEAD 6
#define M2M_DIRECT_PAIRING_FLAG 0
#define M2M_DIRECT_PAIRING_ACK_FLAG 1
#define M2M_DIRECT_KEEPALIVE_FLAG 2
#define M2M_DIRECT_DATA_FLAG 3
#define M2M_DIRECT_SMALL_ARRAY_LIMIT 13

#define MAC_ADDRESS_LENGTH 6
#define ENCRYPTION_KEY_LENGTH 16

#define M2M_DIRECT_DEBUG_SEND
#define M2M_DIRECT_DEBUG_RECEIVE

#define M2M_DIRECT_INDICATOR_LED_INITIALISED_INTERVAL 50
#define M2M_DIRECT_INDICATOR_LED_PAIRING_INTERVAL 100
#define M2M_DIRECT_INDICATOR_LED_PAIRED_INTERVAL 250
#define M2M_DIRECT_INDICATOR_LED_CONNECTING_INTERVAL 500
#define M2M_DIRECT_INDICATOR_LED_CONNECTED_INTERVAL 0
#define M2M_DIRECT_INDICATOR_LED_DISCONNECTED_INTERVAL 75

#define M2M_DIRECT_LINK_QUALITY_UPPER_THRESHOLD 18
#define M2M_DIRECT_LINK_QUALITY_LOWER_THRESHOLD 12

enum class m2mDirectState: std::uint8_t {
	uninitialised,
	initialised,
	started,
	scanning,
	pairing,
	paired,
	connecting,
	connected,
	disconnected
};

bool initialiseEspNowCallbacks();													//Initialise the ESP-Now callbacks

class m2mDirectClass	{

	public:
		m2mDirectClass();																//Constructor function
		~m2mDirectClass();																//Destructor function
		char* localName();															//Returns a pointer to the local device name (or nullptr if not set)
		void localName(char *nameToSet);											//Set a name for the device (optional)
		void localName(String nameToSet);											//Set a name for the device (optional)
		bool remoteNameSet();														//Is the remote name set
		char* remoteName();															//Returns a pointer to the remote device name (or nullptr if not set)
		void disableEncryption();													//Disable encryption (why?)
		void begin(uint8_t communicationChannel = 0, uint8_t pairingChannel = 1);	//Start the m2mDirectClass library
		void pairingButtonGpio(uint8_t pin = 255, bool inverted = false);			//Set pin used for pairing button GPIO
		void indicatorGpio(uint8_t pin = 255, bool inverted = false);				//Set pin used for indicator GPIO
		void housekeeping();														//Maintain keepalives etc.
		m2mDirectClass& setPairingCallback(std::function<void()> function);				//Set the pairing start callback (mostly for information if the pairing button is pushed)
		m2mDirectClass& setPairedCallback(std::function<void()> function);				//Set the paired callback
		m2mDirectClass& setConnectedCallback(std::function<void()> function);			//Set the connected callback
		m2mDirectClass& setDisconnectedCallback(std::function<void()> function);			//Set the disconnected callback
		m2mDirectClass& setMessageReceivedCallback(std::function<void()> function);		//Set the message received callback
		bool connected();															//Simple boolean measure of being connected
		uint32_t linkQuality();														//A measure of link quality
		void setAutomaticTxPower(bool setting = true);								//Enable/disable automatic Tx power
		void debug(Stream &);														//Start debugging on a stream

		bool ICACHE_FLASH_ATTR addStr(char* dataToAdd)								//Specific method to add a null terminated C string, which sorts out null termination
		{
			uint8_t dataType = determineDataType(dataToAdd);
			uint8_t dataLength = strnlen(dataToAdd, 255);	//Pseudo-safe strlen usage that will most likely simply not fit in the buffer instead of causing an exception
			if(_applicationBufferPosition + dataLength + 1 < MAXIMUM_MESSAGE_SIZE - M2M_DIRECT_PACKET_OVERHEAD)
			{
				if(debug_uart_ != nullptr)
				{
					debug_uart_->print(F("\r\nAdding "));
					_dataTypeDescription(DATA_STR);
					debug_uart_->printf_P(PSTR(" %u bytes "), dataLength);
				}
				_applicationPacketBuffer[_applicationBufferPosition++] = DATA_STR;	//Force this to be a null terminated string
				_applicationPacketBuffer[_applicationBufferPosition++] = dataLength;
				memcpy(&_applicationPacketBuffer[_applicationBufferPosition],dataToAdd,dataLength);			//Copy in the data
				if(debug_uart_ != nullptr)
				{
					for(uint8_t index = 0; index < dataLength; index++)
					{
						debug_uart_->print(_applicationPacketBuffer[_applicationBufferPosition+index]);
						debug_uart_->print(' ');
					}
				}
				_applicationBufferPosition+=dataLength;														//Advance the index past the data
				_applicationPacketBuffer[1] = _applicationPacketBuffer[1] + 1;							//Increment the field counter
				return true;
			}
			return false;	//Not enough space left in the packet
		}
		bool ICACHE_FLASH_ATTR add(bool dataToAdd)							//Bool is a special case
		{
			uint8_t dataLength = sizeof(dataToAdd);
			if(_applicationBufferPosition + dataLength < MAXIMUM_MESSAGE_SIZE - M2M_DIRECT_PACKET_OVERHEAD)
			{
				if(debug_uart_ != nullptr)
				{
					debug_uart_->print(F("\r\nAdding bool"));
				}
				if((bool)dataToAdd == true)
				{
					_applicationPacketBuffer[_applicationBufferPosition++] = DATA_BOOL_TRUE;	//True
				}
				else
				{
					_applicationPacketBuffer[_applicationBufferPosition++] = DATA_BOOL;			//False
				}
				_applicationPacketBuffer[1] = _applicationPacketBuffer[1] + 1;						//Increment the field counter
				return true;
			}
			return false;	//Not enough space left in the packet
		}
		bool ICACHE_FLASH_ATTR add(bool* dataToAdd, uint8_t length)							//Generic templated add functions
		{
			uint8_t dataLength = sizeof(bool)*length;
			if(_applicationBufferPosition + dataLength + 1 < MAXIMUM_MESSAGE_SIZE - M2M_DIRECT_PACKET_OVERHEAD)	//Each piece of data has a byte with it showing the type
			{
				if(debug_uart_ != nullptr)
				{
					debug_uart_->print(F("\r\nAdding "));
					_dataTypeDescription(DATA_BOOL_ARRAY);
				}
				if(debug_uart_ != nullptr)
				{
					debug_uart_->printf_P(PSTR("[%u] %u bytes "), length, dataLength);
				}
				_applicationPacketBuffer[_applicationBufferPosition++] = (DATA_BOOL_ARRAY | 0x80);
				_applicationPacketBuffer[_applicationBufferPosition++] = length;
				memcpy(&_applicationPacketBuffer[_applicationBufferPosition],dataToAdd,dataLength);	//Copy in the data
				if(debug_uart_ != nullptr)
				{
					for(uint8_t index = 0; index < dataLength; index++)
					{
						debug_uart_->print(_applicationPacketBuffer[_applicationBufferPosition+index]);
						debug_uart_->print(' ');
					}
				}
				_applicationBufferPosition+=dataLength;												//Advance the index past the data
				_applicationPacketBuffer[1] = _applicationPacketBuffer[1] + 1;										//Increment the field counter
				return true;
			}
			else
			{
				return false;	//Not enough space left in the packet
			}
		}
		template<typename typeToAdd>
		bool ICACHE_FLASH_ATTR add(typeToAdd dataToAdd)							//Generic templated add functions
		{
			uint8_t dataType = determineDataType(dataToAdd);
			uint8_t dataLength = sizeof(dataToAdd);
			if(_applicationBufferPosition + dataLength + 1 < MAXIMUM_MESSAGE_SIZE - M2M_DIRECT_PACKET_OVERHEAD)
			{
				if(debug_uart_ != nullptr)
				{
					debug_uart_->print(F("\r\nAdding "));
					_dataTypeDescription(dataType);
					debug_uart_->printf_P(PSTR(" %u bytes "), dataLength);
				}
				_applicationPacketBuffer[_applicationBufferPosition++] = dataType;
				memcpy(&_applicationPacketBuffer[_applicationBufferPosition],&dataToAdd,dataLength);	//Copy in the data
				if(debug_uart_ != nullptr)
				{
					for(uint8_t index = 0; index < dataLength; index++)
					{
						debug_uart_->print(_applicationPacketBuffer[_applicationBufferPosition+index]);
						debug_uart_->print(' ');
					}
				}
				_applicationBufferPosition+=dataLength;												//Advance the index past the data
				_applicationPacketBuffer[1] = _applicationPacketBuffer[1] + 1;										//Increment the field counter
				return true;
			}
			return false;	//Not enough space left in the packet
		}
		template<typename typeToAdd>
		bool ICACHE_FLASH_ATTR add(typeToAdd dataToAdd, uint8_t length)							//Generic templated add functions
		{
			uint8_t dataType = determineDataType(dataToAdd);
			uint8_t dataLength = determineDataSize(dataToAdd)*length;
			if(_applicationBufferPosition + dataLength + 1 < MAXIMUM_MESSAGE_SIZE - M2M_DIRECT_PACKET_OVERHEAD)	//Each piece of data has a byte with it showing the type
			{
				if(dataType == DATA_BOOL)	//Bool is a special case for packing as it only needs on byte
				{
					if(debug_uart_ != nullptr)
					{
						debug_uart_->printf_P(PSTR("\r\nAdding bool[%u] %u bytes "), length, dataLength);
					}
					_applicationPacketBuffer[_applicationBufferPosition++] = (dataType | 0x80);
					_applicationPacketBuffer[_applicationBufferPosition++] = length;
					for(uint8_t index = 0; index < length; index++)
					{
						if((bool)dataToAdd[index] == true)
						{
							_applicationPacketBuffer[_applicationBufferPosition++] = DATA_BOOL_TRUE;	//True
							if(debug_uart_ != nullptr)
							{
								debug_uart_->print(_applicationPacketBuffer[_applicationBufferPosition - 1]);
							}
						}
						else
						{
							_applicationPacketBuffer[_applicationBufferPosition++] = DATA_BOOL;			//False
							if(debug_uart_ != nullptr)
							{
								debug_uart_->print(_applicationPacketBuffer[_applicationBufferPosition - 1]);
							}
						}
						if(debug_uart_ != nullptr)
						{
							debug_uart_->print(' ');
						}
					}
					_applicationPacketBuffer[1] = _applicationPacketBuffer[1] + 1;						//Increment the field counter
					return true;
				}
				else
				{
					if(debug_uart_ != nullptr)
					{
						debug_uart_->print(F("\r\nAdding "));
						_dataTypeDescription(dataType);
					}
					if(debug_uart_ != nullptr)
					{
						debug_uart_->printf_P(PSTR("[%u] %u bytes "), length, dataLength);
					}
					_applicationPacketBuffer[_applicationBufferPosition++] = (dataType | 0x80);
					_applicationPacketBuffer[_applicationBufferPosition++] = length;
					memcpy(&_applicationPacketBuffer[_applicationBufferPosition],dataToAdd,dataLength);	//Copy in the data
					if(debug_uart_ != nullptr)
					{
						for(uint8_t index = 0; index < dataLength; index++)
						{
							debug_uart_->print(_applicationPacketBuffer[_applicationBufferPosition+index]);
							debug_uart_->print(' ');
						}
					}
					_applicationBufferPosition+=dataLength;												//Advance the index past the data
					_applicationPacketBuffer[1] = _applicationPacketBuffer[1] + 1;										//Increment the field counter
					return true;
				}
			}
			else
			{
				return false;	//Not enough space left in the packet
			}
		}
		//Consider making a variadic function for adding multiple items of heterogenous types in one go https://en.cppreference.com/w/cpp/utility/variadic
		bool sendMessage(bool wait = true);																				//Send the accumulated message
		static const uint8_t DATA_UNAVAILABLE =    0xff;			//Used to denote no more data left, this is never packed in a packet, but can be returned to the application
		static const uint8_t DATA_BOOL =           0x00;			//Used to denote boolean, it also implies the boolean is false
		static const uint8_t DATA_BOOL_TRUE =      0x01;			//Used to denote boolean, it also implies the boolean is true
		static const uint8_t DATA_UINT8_T =        0x02;			//Used to denote an uint8_t in user data
		static const uint8_t DATA_UINT16_T =       0x03;			//Used to denote an uint16_t in user data
		static const uint8_t DATA_UINT32_T =       0x04;			//Used to denote an uint32_t in user data
		static const uint8_t DATA_UINT64_T =       0x05;			//Used to denote an uint64_t in user data
		static const uint8_t DATA_INT8_T =         0x06;			//Used to denote an int8_t in user data
		static const uint8_t DATA_INT16_T =        0x07;			//Used to denote an int16_t in user data
		static const uint8_t DATA_INT32_T =        0x08;			//Used to denote an int32_t in user data
		static const uint8_t DATA_INT64_T =        0x09;			//Used to denote an int64_t in user data
		static const uint8_t DATA_FLOAT =          0x0a;			//Used to denote a float (32-bit) in user data
		static const uint8_t DATA_DOUBLE =         0x0b;			//Used to denote a double float (64-bit) in user data
		static const uint8_t DATA_CHAR =           0x0c;			//Used to denote a char in user data
		static const uint8_t DATA_STR =            0x0d;			//Used to denote a null terminated C string in user data
		static const uint8_t DATA_KEY =            0x0e;			//Used to denote a key, which is a null terminated C string in user data
		static const uint8_t DATA_CUSTOM =         0x0f;			//Used to denote a custom type in user data
		static const uint8_t DATA_BOOL_ARRAY =     0x80;			//Used to denote boolean array in user data
		static const uint8_t DATA_UINT8_T_ARRAY =  0x82;			//Used to denote an uint8_t array in user data
		static const uint8_t DATA_UINT16_T_ARRAY = 0x83;			//Used to denote an uint16_t array in user data
		static const uint8_t DATA_UINT32_T_ARRAY = 0x84;			//Used to denote an uint32_t array in user data
		static const uint8_t DATA_UINT64_T_ARRAY = 0x85;			//Used to denote an uint64_t array in user data
		static const uint8_t DATA_INT8_T_ARRAY =   0x86;			//Used to denote an int8_t array in user data
		static const uint8_t DATA_INT16_T_ARRAY =  0x87;			//Used to denote an int16_t array in user data
		static const uint8_t DATA_INT32_T_ARRAY =  0x88;			//Used to denote an int32_t array in user data
		static const uint8_t DATA_INT64_T_ARRAY =  0x89;			//Used to denote an int64_t array in user data
		static const uint8_t DATA_FLOAT_ARRAY =    0x8a;			//Used to denote a float (32-bit) array in user data
		static const uint8_t DATA_DOUBLE_ARRAY =   0x8b;			//Used to denote a double float (64-bit) array in user data
		static const uint8_t DATA_CHAR_ARRAY =     0x8c;			//Used to denote a char array in user data (not a null terminated C string!)
		static const uint8_t DATA_CUSTOM_ARRAY =   0x8f;			//Used to denote a custom type array in user data
		
		
		bool ICACHE_FLASH_ATTR retrieveStr(char* dataDestination)	//Specific method to retrieve a null terminated C string, which sorts out null termination
		{
			if(_receivedPacketBuffer[1] == 0)
			{
				if(debug_uart_ != nullptr)
				{
					debug_uart_->print(F("\nNo data left to retrieve"));
				}
				return false;
			}
			else
			{
				uint8_t dataType = determineDataType(*dataDestination);
				if((dataType & 0x0f) == DATA_CHAR || (dataType & 0x0f) == DATA_STR)
				{
					_receivedPacketBuffer[1]--;	//Mark the field as retrieved
					_receivedPacketBufferPosition++;	//Skip over the type marker for the field
					uint8_t dataLength = _receivedPacketBuffer[_receivedPacketBufferPosition++];	//Get the length of the char* and move past the length field
					if(dataLength > 0)
					{
						memcpy(dataDestination,&_receivedPacketBuffer[_receivedPacketBufferPosition],dataLength);	//Copy the data
					}
					dataDestination[dataLength]=char(0);	//Null terminate the char*
					if(_receivedPacketBuffer[1] == 0)
					{
						_receivedPacketBufferPosition = 2;		//Reset the buffer position for the next message
					}
					else
					{
						_receivedPacketBufferPosition+=dataLength;	//Move to the next field
					}
					return true;
				}
				else
				{
					if(debug_uart_ != nullptr)
					{
						debug_uart_->print(F("\nWrong data type for retrieval, asked for "));
						_dataTypeDescription(dataType);
						debug_uart_->print(F(" packet has "));
						_dataTypeDescription(_receivedPacketBuffer[_receivedPacketBufferPosition]);

					}
					return false;
				}
			}
		}
		bool ICACHE_FLASH_ATTR retrieve(bool *dataDestination, uint8_t length = 1)			//Bool is a special case
		{
			if(_receivedPacketBuffer[1] == 0)
			{
				if(debug_uart_ != nullptr)
				{
					debug_uart_->print(F("\nNo data left to retrieve"));
				}
				return false;
			}
			else
			{
				uint8_t dataType = determineDataType(*dataDestination);
				if(length > 1)	//Force it to be an array
				{
					dataType = dataType | 0x80;
				}
				if(dataType == DATA_BOOL)
				{
					if(_receivedPacketBuffer[_receivedPacketBufferPosition] == DATA_BOOL_TRUE)
					{
						*dataDestination = true;
					}
					else
					{
						*dataDestination = false;
					}
					_receivedPacketBuffer[1]--;	//Mark the field as retrieved
					if(_receivedPacketBuffer[1] == 0)
					{
						_receivedPacketBufferPosition = 2;	//Reset the buffer position for the next message
					}
					else
					{
						_receivedPacketBufferPosition++;	//Move to the next field
					}
					return true;
				}
				else if(dataType == (_receivedPacketBuffer[_receivedPacketBufferPosition] & 0x8f))	//Strip off any array length
				{
					uint8_t dataLength = 0;
					if(debug_uart_ != nullptr)
					{
						debug_uart_->print(F("\r\nRetrieving "));
						_dataTypeDescription(dataType);
					}
					if((_receivedPacketBuffer[_receivedPacketBufferPosition] & 0xf0) == 0 && length == 1) //It's not an array
					{
						_receivedPacketBuffer[1]--;	//Mark the field as retrieved
						_receivedPacketBufferPosition++;	//Skip over the type marker for the field
						dataLength = sizeof(bool);	//How long is this?
					}
					else if(_receivedPacketBuffer[_receivedPacketBufferPosition+1] == length)	//It's an array and the application is expecting the right length
					{
						_receivedPacketBuffer[1]--;	//Mark the field as retrieved
						_receivedPacketBufferPosition++;	//Skip over the type marker for the field
						dataLength = sizeof(bool) * _receivedPacketBuffer[_receivedPacketBufferPosition];	//How long is this?
						_receivedPacketBufferPosition++;	//Skip over the length marker for the field
						if(debug_uart_ != nullptr)
						{
							debug_uart_->print('[');
							debug_uart_->print(length);
							debug_uart_->print(']');
							debug_uart_->print(' ');
							debug_uart_->print(dataLength);
							debug_uart_->print(F(" bytes"));
						}
					}
					else
					{
						if(debug_uart_ != nullptr)
						{
							debug_uart_->print(F(" failed"));
						}
						return false; //Do nothing and fail
					}
					memcpy(dataDestination,&_receivedPacketBuffer[_receivedPacketBufferPosition],dataLength);	//Copy the data
					if(_receivedPacketBuffer[1] == 0)
					{
						_receivedPacketBufferPosition = 2;		//Reset the buffer position for the next message
					}
					else
					{
						_receivedPacketBufferPosition+=dataLength;	//Move to the next field
					}
					return true;
				}
				else
				{
					if(debug_uart_ != nullptr)
					{
						debug_uart_->print(F("\nWrong data type for retrieval, asked for "));
						_dataTypeDescription(dataType);
						debug_uart_->print(F(" packet has "));
						_dataTypeDescription(_receivedPacketBuffer[_receivedPacketBufferPosition]);

					}
					return false;
				}
			}
		}
		template<typename typeToRetrieve>
		bool ICACHE_FLASH_ATTR retrieve(typeToRetrieve *dataDestination, uint8_t length = 1)			//Generic templated retrieve functions
		{
			if(_receivedPacketBuffer[1] == 0)
			{
				if(debug_uart_ != nullptr)
				{
					debug_uart_->print(F("\nNo data left to retrieve"));
				}
				return false;
			}
			else
			{
				uint8_t dataType = determineDataType(*dataDestination);
				if(length > 1)	//Force it to be an array
				{
					dataType = dataType | 0x80;
				}
				if(dataType == (_receivedPacketBuffer[_receivedPacketBufferPosition] & 0x8f))	//Strip off any array length
				{
					uint8_t dataLength = 0;
					if(debug_uart_ != nullptr)
					{
						debug_uart_->print(F("\r\nRetrieving "));
						_dataTypeDescription(dataType);
					}
					if((_receivedPacketBuffer[_receivedPacketBufferPosition] & 0xf0) == 0 && length == 1) //It's not an array
					{
						_receivedPacketBuffer[1]--;	//Mark the field as retrieved
						_receivedPacketBufferPosition++;	//Skip over the type marker for the field
						dataLength = sizeof(typeToRetrieve);	//How long is this?
					}
					else if(_receivedPacketBuffer[_receivedPacketBufferPosition+1] == length)	//It's an array and the application is expecting the right length
					{
						_receivedPacketBuffer[1]--;	//Mark the field as retrieved
						_receivedPacketBufferPosition++;	//Skip over the type marker for the field
						dataLength = sizeof(typeToRetrieve) * _receivedPacketBuffer[_receivedPacketBufferPosition];	//How long is this?
						_receivedPacketBufferPosition++;	//Skip over the length marker for the field
						if(debug_uart_ != nullptr)
						{
							debug_uart_->print('[');
							debug_uart_->print(length);
							debug_uart_->print(']');
							debug_uart_->print(' ');
							debug_uart_->print(dataLength);
							debug_uart_->print(F(" bytes"));
						}
					}
					else
					{
						if(debug_uart_ != nullptr)
						{
							debug_uart_->print(F(" failed"));
						}
						return false; //Do nothing and fail
					}
					memcpy(dataDestination,&_receivedPacketBuffer[_receivedPacketBufferPosition],dataLength);	//Copy the data
					if(_receivedPacketBuffer[1] == 0)
					{
						_receivedPacketBufferPosition = 2;		//Reset the buffer position for the next message
					}
					else
					{
						_receivedPacketBufferPosition+=dataLength;	//Move to the next field
					}
					return true;
				}
				else
				{
					if(debug_uart_ != nullptr)
					{
						debug_uart_->print(F("\nWrong data type for retrieval, asked for "));
						_dataTypeDescription(dataType);
						debug_uart_->print(F(" packet has "));
						_dataTypeDescription(_receivedPacketBuffer[_receivedPacketBufferPosition]);

					}
					return false;
				}
			}
		}
		uint8_t dataAvailable();													//Number of fields left in the message
		uint8_t nextDataType();														//Return the 'type' of the next piece of data
		uint8_t nextDataLength();													//Return the 'length' of the next piece of data, for C strings, Strings etc.
		void skipReceivedData();													//Skips a data field
		void clearReceivedMessage();												//Clear any received message, even if not all read
		bool resetPairing();														//Reset pairing info and reset state to re-pair
	protected:
	private:
		//Variables
		#if defined ESP8266
			//Ticker houseKeepingticker;													//The Ticker used to run regular housekeeping tasks
		#elif defined ESP32
			Preferences settings;													//Instance of preferences used to store settings
			char preferencesNamespace[10] = "m2mDirect";							//Preferences namespace used to store pairing info
			char pairedMacKey[8] = "pairMac";										//Key in namespace for paired MAC address
			char pairedPrimaryKey[7] = "priKey";									//Key in namespace for primary encryption key
			char pairedLocalKey[7] = "locKey";										//Key in namespace for local encryption key
			char pairedNameKey[5] = "name";											//Key in namespace for remote device name
			char pairedNameLengthKey[4] = "len";									//Key in namespace for remote device name length
		#endif
		Stream *debug_uart_ = nullptr;												//The stream used for the debugging
		uint8_t _pairingButtonGpio = 255;											//The GPIO pin used as a pairing button 255=unused
		bool _pairingButtonGpioNc = false;											//GPIO button pin is normally closed
		uint32_t _pairingButtonPressTime = 0;										//Used to detect a long press
		uint8_t _indicatorLedGpio = 255;											//The GPIO pin used as an indicator LED 255=unused
		bool _indicatorLedGpioInverted = false;										//GPIO pin is inverted
		bool _indicatorState = false;												//State of indicator LED
		uint32_t _indicatorTimer = 0;												//Time of last state change of indicator LED
		uint32_t _indicatorTimerInterval = 0;										//How often the indicator changes state
		m2mDirectState state = m2mDirectState::uninitialised;						//State of the connection
		uint8_t _pairingChannel = 0;												//Channel used for pairing
		uint8_t _communicationChannel = 0;											//Channel used for communication
		bool _encyptionEnabled = true;												//Whether to encrypt communication
		uint32_t _localActivityTimer = 0;											//General timer for periodic activity like keepalives
		uint32_t _previouslocalActivityTimer = 0;									//Need the previous timer value for checking keepalive echoes
		uint32_t _remoteActivityTimer = 0;											//General timer for periodic activity like keepalives
		uint32_t receivedLocalActivityTimer = 0;									//Used in echo quality detection
		//uint32_t _lastTimestamp = 0;												//Last timestamp in a sent packet
		uint32_t _startingKeepaliveInterval = 250;									//Starting keepalive time for a paired connection
		uint32_t _minimumKeepaliveInterval = 50;									//Minimum keepalive time for a paired connection
		uint32_t _maximumKeepaliveInterval = 100000000;									//Maximum keepalive time for a paired connection
		uint32_t _keepaliveInterval = 250;											//Keepalive time for a paired connection
		uint32_t _pairingInterval = 5000;											//How often to send pairing packets
		uint32_t _sendTimer = 0;													//Timer for sent packets
		uint32_t _sendTimeout = 100;												//How long to wait for confirmation of a sent packet
		bool _waitingForSendCallback = false;										//Flag that we're waiting for a callback
		uint32_t _sendQuality = 0x00000000;											//A measure of send quality, using built in ACKs from ESP-Now
		uint32_t _startingSendquality = 0x00000000;
		uint32_t _echoQuality = 0x00000000;											//A measure of echo quality, using keepalive echoes
		uint32_t _startingEchoQuality = 0x00000000;
		bool _automaticTxPower = true;												//Enable/disable automatic TxPower
		int8_t _currentTxPower = 0;													//Current Tx power
		int8_t _minTxPower = 9;														//Minimum Tx power 20dBm (80 * 0.25)
		int8_t _maxTxPower = 80;													//Maximum Tx power 20dBm (80 * 0.25)
		uint32_t _lastTxPowerChange = 0;
		bool _lastTxPowerChangeDownwards = false;
		uint8_t _primaryEncryptionKey[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};		//Primary encryption key
		uint8_t _localEncryptionKey[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};		//Encryption key for this device
		uint8_t _localMacAddress[6] = {0, 0, 0, 0, 0, 0};							//MAC address of this device
		uint8_t _remoteMacAddress[6] = {0, 0, 0, 0, 0, 0};							//MAC address of paired device
		//uint8_t _remoteEncryptionKey[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};		//Encryption key of paired device
		uint8_t _broadcastMacAddress[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};			//Broadcast MAC address, used for pairing
		char* localDeviceName = nullptr;
		char* remoteDeviceName = nullptr;
		bool _pairingInfoRead = false;
		bool _pairingInfoWritten = false;
		//Packet buffers
		uint8_t _protocolPacketBuffer[MAXIMUM_MESSAGE_SIZE];						//Packet buffer for m2mDirect protocol packets, pairing, naming etc.
		uint8_t _protocolPacketBufferPosition = 0;
		uint8_t _applicationPacketBuffer[MAXIMUM_MESSAGE_SIZE];						//Packet buffer for application data packets
		uint8_t _applicationBufferPosition = 2;
		uint8_t _receivedPacketBuffer[MAXIMUM_MESSAGE_SIZE];						//Packet buffer for application data packets
		uint8_t _receivedPacketBufferPosition = 2;									//Position in received data buffer
		bool _dataReceived = false;													//Flag to trigger callback when data is received
		//Callbacks
		std::function<void()> pairingCallback = nullptr;							//Pointer to the pairing start callback
		std::function<void()> pairedCallback = nullptr;								//Pointer to the paired callback
		std::function<void()> connectedCallback = nullptr;							//Pointer to the connected callback
		std::function<void()> disconnectedCallback = nullptr;						//Pointer to the disconnected callback
		std::function<void()> messageReceivedCallback = nullptr;					//Pointer to the message received callback
		//Methods
		void _advanceTimers();														//Swap current/previous activity timers
		bool _readPairingInfo();													//Read pairing from EEPROM (ESP8266) or 'preferences' (ESP32)
		bool _writePairingInfo();													//Write pairing from EEPROM (ESP8266) or 'preferences' (ESP32)
		bool _deletePairingInfo();													//Delete pairing from EEPROM (ESP8266) or 'preferences' (ESP32)
		bool _initialiseWiFi();														//Initialise the WiFi interface, which varies depending on if connected etc.
		bool _initialiseEspNow(uint8_t channel);									//Initialise ESP-Now
		bool _initialiseEspNowCallbacks();											//Initialise the ESP-Now callbacks
		uint8_t _leastCongestedChannel();											//Scan the neighbourhood for the least congested channel with a dumb heuristic
		bool _changeChannel(uint8_t channel);										//Change the channel
		void _chooseEncryptionKeys();												//Choose encryption keys
		void _clearEncryptionKeys();												//Clear encryption keys
		bool _setPrimaryEncryptionKey();											//Set the primary encryption key
		void _createPairingMessage();												//Create the pairing message
		void _createPairingAckMessage();											//Create the pairing ACK message
		void _increaseKeepaliveInterval();											//Increase keepalive interval
		void _decreaseKeepaliveInterval();											//Increase keepalive interval
		void _createKeepaliveMessage();												//Create the connection keepalive message
		bool _sendBroadcastPacket(uint8_t* buffer, uint8_t length);					//Send broadcast messages, mostly for pairing
		bool _sendUnicastPacket(uint8_t* buffer, uint8_t length, bool wait = true);	//Send unicast messages
		uint8_t _countBits(uint32_t);												//Count the number of set bits in an uint32_t
		bool _registerPeer(uint8_t* macaddress, uint8_t channel);					//Register an unencrypted peer
		bool _registerPeer(uint8_t* macaddress, uint8_t channel, uint8_t* key);		//Register an encrypted peer
		void _printPacketDescription(uint8_t);										//Print packet description
		void _printCurrentState();	
		void _debugState();	
		uint8_t _currentChannel();													//Current WiFi channel
		uint8_t ICACHE_FLASH_ATTR determineDataType(bool type)						{return(DATA_BOOL			);}
		uint8_t ICACHE_FLASH_ATTR determineDataType(bool* type)						{return(DATA_BOOL_ARRAY		);}
		uint8_t ICACHE_FLASH_ATTR determineDataType(uint8_t type)					{return(DATA_UINT8_T		);}
		uint8_t ICACHE_FLASH_ATTR determineDataType(uint8_t* type)					{return(DATA_UINT8_T_ARRAY	);}
		uint8_t ICACHE_FLASH_ATTR determineDataType(int8_t type)					{return(DATA_INT8_T			);}
		uint8_t ICACHE_FLASH_ATTR determineDataType(int8_t* type)					{return(DATA_INT8_T_ARRAY	);}
		uint8_t ICACHE_FLASH_ATTR determineDataType(uint16_t type)					{return(DATA_UINT16_T		);}
		uint8_t ICACHE_FLASH_ATTR determineDataType(uint16_t* type)					{return(DATA_UINT16_T_ARRAY	);}
		uint8_t ICACHE_FLASH_ATTR determineDataType(int16_t type)					{return(DATA_INT16_T		);}
		uint8_t ICACHE_FLASH_ATTR determineDataType(int16_t* type)					{return(DATA_INT16_T_ARRAY	);}
		uint8_t ICACHE_FLASH_ATTR determineDataType(uint32_t type)					{return(DATA_UINT32_T		);}
		uint8_t ICACHE_FLASH_ATTR determineDataType(uint32_t* type)					{return(DATA_UINT32_T_ARRAY	);}
		uint8_t ICACHE_FLASH_ATTR determineDataType(int32_t type)					{return(DATA_INT32_T		);}
		uint8_t ICACHE_FLASH_ATTR determineDataType(int32_t* type)					{return(DATA_INT32_T_ARRAY	);}
		uint8_t ICACHE_FLASH_ATTR determineDataType(uint64_t type)					{return(DATA_UINT64_T		);}
		uint8_t ICACHE_FLASH_ATTR determineDataType(uint64_t* type)					{return(DATA_UINT64_T_ARRAY	);}
		uint8_t ICACHE_FLASH_ATTR determineDataType(int64_t type)					{return(DATA_INT64_T		);}
		uint8_t ICACHE_FLASH_ATTR determineDataType(int64_t* type)					{return(DATA_INT64_T_ARRAY	);}
		uint8_t ICACHE_FLASH_ATTR determineDataType(float type)						{return(DATA_FLOAT			);}
		uint8_t ICACHE_FLASH_ATTR determineDataType(float* type)					{return(DATA_FLOAT_ARRAY	);}
		uint8_t ICACHE_FLASH_ATTR determineDataType(double type)					{return(DATA_DOUBLE			);}
		uint8_t ICACHE_FLASH_ATTR determineDataType(double* type)					{return(DATA_DOUBLE_ARRAY	);}
		uint8_t ICACHE_FLASH_ATTR determineDataType(char type)						{return(DATA_CHAR			);}
		uint8_t ICACHE_FLASH_ATTR determineDataType(char* type)						{return(DATA_CHAR_ARRAY		);}
		template<typename customType> uint8_t determineDataType(customType type)	{return(DATA_CUSTOM			);}	//Catchall for custom types, which are probably a structs

		uint8_t ICACHE_FLASH_ATTR determineDataSize(uint8_t type)					{return(sizeof(uint8_t)		);}
		uint8_t ICACHE_FLASH_ATTR determineDataSize(int8_t type)					{return(sizeof(int8_t)		);}
		uint8_t ICACHE_FLASH_ATTR determineDataSize(uint16_t type)					{return(sizeof(uint16_t)	);}
		uint8_t ICACHE_FLASH_ATTR determineDataSize(int16_t type)					{return(sizeof(int16_t)		);}
		uint8_t ICACHE_FLASH_ATTR determineDataSize(uint32_t type)					{return(sizeof(uint32_t)	);}
		uint8_t ICACHE_FLASH_ATTR determineDataSize(int32_t type)					{return(sizeof(int32_t)		);}
		uint8_t ICACHE_FLASH_ATTR determineDataSize(uint64_t type)					{return(sizeof(uint64_t)	);}
		uint8_t ICACHE_FLASH_ATTR determineDataSize(int64_t type)					{return(sizeof(int64_t)		);}
		uint8_t ICACHE_FLASH_ATTR determineDataSize(float type)						{return(sizeof(float)		);}
		uint8_t ICACHE_FLASH_ATTR determineDataSize(double type)					{return(sizeof(double)		);}
		uint8_t ICACHE_FLASH_ATTR determineDataSize(char type)						{return(sizeof(char)		);}
		template<typename customType> uint8_t determineDataSize(customType type)	{return(sizeof(customType)	);}	//Catchall for custom types, which are probably a structs

		uint8_t ICACHE_FLASH_ATTR determineDataSize(bool* type)		{return(sizeof(bool)		);}
		uint8_t ICACHE_FLASH_ATTR determineDataSize(uint8_t* type)	{return(sizeof(uint8_t)		);}
		uint8_t ICACHE_FLASH_ATTR determineDataSize(int8_t* type)	{return(sizeof(int8_t)		);}
		uint8_t ICACHE_FLASH_ATTR determineDataSize(uint16_t* type)	{return(sizeof(uint16_t)	);}
		uint8_t ICACHE_FLASH_ATTR determineDataSize(int16_t* type)	{return(sizeof(int16_t)		);}
		uint8_t ICACHE_FLASH_ATTR determineDataSize(uint32_t* type)	{return(sizeof(uint32_t)	);}
		uint8_t ICACHE_FLASH_ATTR determineDataSize(int32_t* type)	{return(sizeof(int32_t)		);}
		uint8_t ICACHE_FLASH_ATTR determineDataSize(uint64_t* type)	{return(sizeof(uint64_t)	);}
		uint8_t ICACHE_FLASH_ATTR determineDataSize(int64_t* type)	{return(sizeof(int64_t)		);}
		uint8_t ICACHE_FLASH_ATTR determineDataSize(float* type)	{return(sizeof(float)		);}
		uint8_t ICACHE_FLASH_ATTR determineDataSize(double* type)	{return(sizeof(double)		);}
		uint8_t ICACHE_FLASH_ATTR determineDataSize(char* type)		{return(sizeof(char)		);}		

		void _dataTypeDescription(uint8_t type);
		bool _tieBreak(uint8_t* macAddress1, uint8_t* macAddress2);					//Tie break between two MAC addresses
		bool _remoteMacAddressSet();												//Returns true if the remote MAC address is confirmed
		void _indicatorOn();
		void _indicatorOff();
		bool _reduceTxPower();														//Reduce the Tx power
		bool _increaseTxPower();													//Increase the Tx power
};
extern m2mDirectClass m2mDirect;	//Create an instance of the class, as only one is practically usable at a time
#endif
