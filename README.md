# m2mDirect
## Introduction

An Arduino library for connecting and maintaining a point-to-point connection between two (and only two) Espressif ESP8266/8285/32 microcontrollers with ESP-NOW. This library uses the inbuilt encryption features of ESP-NOW to provide a minimal level of privacy and attempts to ensure delivery of packets and monitor the quality of the connection.

It should not be considered 'secure' or 'reliable' for serious use but once paired should be more robust than the typical 'fire and forget' approaches to using ESP-NOW.

The pairing process shares randomly generated encryption keys in an insecure manner but this only happens during pairing. Once paired the two devices store this in flash/EEPROM and do not share them again unless re-flashed or paired again.

## What to use this library for

This library is for when you want a low-ish latency two way communication method for sending small amounts of data between two (and only two) microcontrollers and there is no 'infrastructure' such as Wi-Fi access points, MQTT servers etc. to rely on.

It was written to provide similar functionality to hobby remote control devices with return telemetry in a robot project and works well for this.

## What not to use this library for

This library is not suited to to transferring larger amounts of data such as files, pictures, etc. or anything needing strong encryption or guaranteed delivery of packets.

While the link failure detection is reasonably robust you should not rely on it for safety critical projects eg. the control of a heavy/fast/dangerous device like a large quadcopter or rover.

## General description/behaviour

### Pairing

When set to pair, both devices will share information about themselves on Wi-Fi channel 1, in the clear unencrypted. You should not re-pair unless absolutely necessary as it exposes the encryption keys.

There is a method in the library to directly trigger re-pairing from the user application, but also to configure a button on a GPIO pin that will do the same.

There is a method in the library to configure an indicator on a GPIO pin (typically for an LED) that shows the pairing state and link quality. If configured, the link LED behaves in the following way.

- Solid - no link or unpaired
- Slow flashing - paired, linked and reliable
- Fast flashing - actively pairing or unreliable link

### Channel usage

This library will choose a 2.4Ghz channel on pairing from channels 1, 6 & 11 based on the least number of BSSIDs visible on that channel during pairing and re-use that choice every time. It can optionally be configured to re-scan channels on start-up, but this will slow down the connection process.

You can optionally hard configure a fixed channel in the range 1-14, but be aware channels 12-13 are not legal in most countries (legal at reduced power in the US) and channel 14 is only legal in Japan.

### Transmission power management

This library will moderate transmission power downwards when transmitted packets are 100% successful and moderate it upwards when they are not. This is to attempt to be a 'good neighbour' in the often crowded 2.4Ghz space.

Optionally, transmit power can be fixed.

### Reliability

By default this library uses the receive callback feature in ESP-NOW to confirm the other end of the link has received the sent data. This is not 100% reliable but is a fair indication of delivery. As a result the default sending method is synchronous and does not return instantly. The timeout for this can be adjusted.

Optionally, data can be sent asynchronously and the application check for delivery later.

The library also sends periodic keepalives to provide a measure of link reliability. Link reliability is measured as a 32-bit bitfield with the most significant bit being the most recent.

All packets have a rotating sequence number, recent missed packets will be detected and reduce the measure of link quality, even if at time of sending they were apparently delivered.

### Link payload

This library uses two bytes in each packet for signalling, reducing the effective packet size for user data to 248 bytes. If more payload than this is needed, it is for the application to fragment the data.

Future versions of the library may buffer application payload, removing this work from the user application.

## Methods

### Pairing

### Channel usage

### Transmission power management

### Reliability

### Link payload
