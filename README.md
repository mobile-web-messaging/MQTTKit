# MQTTKit

MQTTKit is a modern event-driven Objective-C library for [MQTT 3.1][mqtt].

It uses [Mosquitto 1.2.3](http://mosquitto.org) library.

An iOS application using MQTTKit is available at [MQTTExample](https://github.com/jmesnil/MQTTExample).

# Project Status

__This project is _no longer maintained_ ([some context about this decision](http://jmesnil.net/weblog/2015/09/04/stepping-out-from-personal-open-source-projects/)).__

__If you encounter bugs with it or need enhancements, you can fork it and modify it as the project is under the Apache License 2.0.__

[![Build Status](https://travis-ci.org/mobile-web-messaging/MQTTKit.svg?branch=master)](https://travis-ci.org/mobile-web-messaging/MQTTKit)

## Installation Using CocoaPods

On your ```Podfile``` add this project:

```
...
pod 'MQTTKit', :git => 'https://github.com/mobile-web-messaging/MQTTKit.git'
...
```

For the first time, run ```pod install```, if you are updating the project invoke ```pod update```.

## Usage

Import the `MQTTKit.h` header file

```objc
#import <MQTTKit.h>
```

### Send a Message

```objc
// create the client with a unique client ID
NSString *clientID = ...
MQTTClient *client = [[MQTTClient alloc] initWithClientId:clientID];

// connect to the MQTT server
[self.client connectToHost:@"iot.eclipse.org" 
         completionHandler:^(NSUInteger code) {
    if (code == ConnectionAccepted) {
        // when the client is connected, send a MQTT message
        [self.client publishString:@"Hello, MQTT"
                           toTopic:@"/MQTTKit/example"
                           withQos:AtMostOnce
                            retain:NO
                 completionHandler:^(int mid) {
            NSLog(@"message has been delivered");
        }];
    }
}];

```

### Subscribe to a Topic and Receive Messages

```objc

// define the handler that will be called when MQTT messages are received by the client
[self.client setMessageHandler:^(MQTTMessage *message) {
    NSString *text = [message.payloadString];
    NSLog(@"received message %@", text);
}];

// connect the MQTT client
[self.client connectToHost:@"iot.eclipse.org"
         completionHandler:^(MQTTConnectionReturnCode code) {
    if (code == ConnectionAccepted) {
        // when the client is connected, subscribe to the topic to receive message.
        [self.client subscribe:@"/MQTTKit/example"
         withCompletionHandler:nil];
    }
}];
```

### Disconnect from the server

```objc
[self.client disconnectWithCompletionHandler:^(NSUInteger code) {
    // The client is disconnected when this completion handler is called
    NSLog(@"MQTT client is disconnected");
}];
```
## Authors

* [Jeff Mesnil](http://jmesnil.net/)

[mqtt]: http://public.dhe.ibm.com/software/dw/webservices/ws-mqtt/mqtt-v3r1.html
