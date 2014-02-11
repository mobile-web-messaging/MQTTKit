//
//  MQTTKit.h
//  MQTTKit
//
//  Created by Jeff Mesnil on 22/10/2013.
//  Copyright (c) 2013 Jeff Mesnil. All rights reserved.
//  Copyright 2012 Nicholas Humfrey. All rights reserved.
//

@interface MQTTMessage : NSObject

@property (readonly, assign) unsigned short mid;
@property (readonly, copy) NSString *topic;
@property (readonly, copy) NSData *payload;
@property (readonly, assign) BOOL retained;

- (NSString *)payloadString;

@end

@class MQTTClient;

@protocol MQTTClientDelegate <NSObject>

- (void) client:(MQTTClient *)client didConnect: (NSUInteger)code;

@optional

- (void) client:(MQTTClient *)client didDisconnect: (NSUInteger)code;

- (void) client:(MQTTClient *)client didPublish: (NSUInteger)messageID;

- (void) client:(MQTTClient *)client didReceiveMessage: (MQTTMessage*)message;

- (void) client:(MQTTClient *)client didSubscribe: (NSUInteger)messageID grantedQos:(NSArray*)qos;
- (void) client:(MQTTClient *)client didUnsubscribe: (NSUInteger)messageID;

@end


@interface MQTTClient : NSObject {
    struct mosquitto *mosq;
}

@property (readwrite, copy) NSString *clientID;
@property (readwrite, copy) NSString *host;
@property (readwrite, assign) unsigned short port;
@property (readwrite, copy) NSString *username;
@property (readwrite, copy) NSString *password;
@property (readwrite, assign) unsigned short keepAlive;
@property (readwrite, assign) BOOL cleanSession;
@property (nonatomic, weak) id<MQTTClientDelegate> delegate;

+ (void) initialize;
+ (NSString*) version;

- (MQTTClient*) initWithClientId: (NSString *)clientId;
- (void) setMessageRetry: (NSUInteger)seconds;
- (void) connect;
- (void) connectToHost: (NSString*)host;
- (void) reconnect;
- (void) disconnect;

- (void)setWillData:(NSData *)payload toTopic:(NSString *)willTopic withQos:(NSUInteger)willQos retain:(BOOL)retain;
- (void)setWill:(NSString *)payload toTopic:(NSString *)willTopic withQos:(NSUInteger)willQos retain:(BOOL)retain;
- (void)clearWill;

- (void)publishData:(NSData *)payload toTopic:(NSString *)topic withQos:(NSUInteger)qos retain:(BOOL)retain;
- (void)publishString:(NSString *)payload toTopic:(NSString *)topic withQos:(NSUInteger)qos retain:(BOOL)retain;

- (void)subscribe: (NSString *)topic;
- (void)subscribe: (NSString *)topic withQos:(NSUInteger)qos;
- (void)unsubscribe: (NSString *)topic;

@end
