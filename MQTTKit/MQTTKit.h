//
//  MQTTKit.h
//  MQTTKit
//
//  Created by Jeff Mesnil on 22/10/2013.
//  Copyright (c) 2013 Jeff Mesnil. All rights reserved.
//  Copyright 2012 Nicholas Humfrey. All rights reserved.
//

typedef enum MQTTConnectionReturnCode : NSUInteger {
    ConnectionAccepted,
    ConnectionRefusedUnacceptableProtocolVersion,
    ConnectionRefusedIdentiferRejected,
    ConnectionRefusedServerUnavailable,
    ConnectionRefusedBadUserNameOrPassword,
    ConnectionRefusedNotAuthorized
} MQTTConnectionReturnCode;

typedef enum MQTTQualityOfService : NSUInteger {
    AtMostOnce,
    AtLeastOnce,
    ExactlyOnce
} MQTTQualityOfService;

#pragma mark - MQTT Message

@interface MQTTMessage : NSObject

@property (readonly, assign) unsigned short mid;
@property (readonly, copy) NSString *topic;
@property (readonly, copy) NSData *payload;
@property (readonly, assign) BOOL retained;

- (NSString *)payloadString;

@end

typedef void (^MQTTSubscriptionCompletionHandler)(NSArray *grantedQos);
typedef void (^MQTTMessageHandler)(MQTTMessage *message);
typedef void (^MQTTDisconnectionHandler)(NSUInteger code);

#pragma mark - MQTT Client

@class MQTTClient;

@interface MQTTClient : NSObject {
    struct mosquitto *mosq;
}

@property (readwrite, copy) NSString *clientID;
@property (readwrite, copy) NSString *host;
@property (readwrite, assign) unsigned short port;
@property (readwrite, copy) NSString *username;
@property (readwrite, copy) NSString *password;
@property (readwrite, assign) unsigned short keepAlive;
@property (readwrite, assign) unsigned int reconnectDelay; // in seconds (default is 1)
@property (readwrite, assign) unsigned int reconnectDelayMax; // in seconds (default is 1)
@property (readwrite, assign) BOOL reconnectExponentialBackoff; // wheter to backoff exponentially the reconnect attempts (default is NO)
@property (readwrite, assign) BOOL cleanSession;
@property (readonly, assign) BOOL connected;
@property (nonatomic, copy) MQTTMessageHandler messageHandler;
@property (nonatomic, copy) MQTTDisconnectionHandler disconnectionHandler;

+ (void) initialize;
+ (NSString*) version;

- (MQTTClient*) initWithClientId: (NSString *)clientId;
- (MQTTClient*) initWithClientId: (NSString *)clientId
                    cleanSession: (BOOL )cleanSession;

- (void) setMaxInflightMessages:(NSUInteger)maxInflightMessages;
- (void) setMessageRetry: (NSUInteger)seconds;

#pragma mark - Connection

- (void) connectWithCompletionHandler:(void (^)(MQTTConnectionReturnCode code))completionHandler;
- (void) connectToHost: (NSString*)host
     completionHandler:(void (^)(MQTTConnectionReturnCode code))completionHandler;
- (void) disconnectWithCompletionHandler:(MQTTDisconnectionHandler)completionHandler;
- (void) reconnect;
- (void)setWillData:(NSData *)payload
            toTopic:(NSString *)willTopic
            withQos:(MQTTQualityOfService)willQos
             retain:(BOOL)retain;
- (void)setWill:(NSString *)payload
        toTopic:(NSString *)willTopic
        withQos:(MQTTQualityOfService)willQos
         retain:(BOOL)retain;
- (void)clearWill;

#pragma mark - Publish

- (void)publishData:(NSData *)payload
            toTopic:(NSString *)topic
            withQos:(MQTTQualityOfService)qos
             retain:(BOOL)retain
  completionHandler:(void (^)(int mid))completionHandler;
- (void)publishString:(NSString *)payload
              toTopic:(NSString *)topic
              withQos:(MQTTQualityOfService)qos
               retain:(BOOL)retain
    completionHandler:(void (^)(int mid))completionHandler;

#pragma mark - Subscribe

- (void)subscribe:(NSString *)topic
withCompletionHandler:(MQTTSubscriptionCompletionHandler)completionHandler;
- (void)subscribe:(NSString *)topic
          withQos:(MQTTQualityOfService)qos
completionHandler:(MQTTSubscriptionCompletionHandler)completionHandler;
- (void)unsubscribe: (NSString *)topic
withCompletionHandler:(void (^)(void))completionHandler;

@end
