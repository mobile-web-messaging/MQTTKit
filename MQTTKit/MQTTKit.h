//
//  MQTTKit.h
//  MQTTKit
//
//  Created by Jeff Mesnil on 22/10/2013.
//  Copyright (c) 2013 Jeff Mesnil. All rights reserved.
//  Copyright 2012 Nicholas Humfrey. All rights reserved.
//

@interface MQTTMessage : NSObject
{
    unsigned short mid;
    NSString *topic;
    NSString *payload;
    unsigned short payloadlen;
    unsigned short qos;
    BOOL retained;
}


@property (readwrite, assign) unsigned short mid;
@property (readwrite, retain) NSString *topic;
@property (readwrite, retain) NSString *payload;
@property (readwrite, assign) unsigned short payloadlen;
@property (readwrite, assign) unsigned short qos;
@property (readwrite, assign) BOOL retained;

-(id)init;

@end

@class MQTTClient;

@protocol MQTTClientDelegate

- (void) client:(MQTTClient *)client didConnect: (NSUInteger)code;
- (void) client:(MQTTClient *)client didDisconnect: (NSUInteger)code;
- (void) client:(MQTTClient *)client didPublish: (NSUInteger)messageID;

- (void) client:(MQTTClient *)client didReceiveMessage: (MQTTMessage*)message;
- (void) client:(MQTTClient *)client didSubscribe: (NSUInteger)messageID grantedQos:(NSArray*)qos;
- (void) client:(MQTTClient *)client didUnsubscribe: (NSUInteger)messageID;

@end


@interface MQTTClient : NSObject {
    struct mosquitto *mosq;
    NSString *host;
    unsigned short port;
    NSString *username;
    NSString *password;
    unsigned short keepAlive;
    BOOL cleanSession;

    __unsafe_unretained id<MQTTClientDelegate> delegate;
    NSTimer *timer;
}

@property (readwrite,retain) NSString *host;
@property (readwrite,assign) unsigned short port;
@property (readwrite,retain) NSString *username;
@property (readwrite,retain) NSString *password;
@property (readwrite,assign) unsigned short keepAlive;
@property (readwrite,assign) BOOL cleanSession;
@property (readwrite,assign) id<MQTTClientDelegate> delegate;

+ (void) initialize;
+ (NSString*) version;


- (MQTTClient*) initWithClientId: (NSString *)clientId;
- (void) setMessageRetry: (NSUInteger)seconds;
- (void) connect;
- (void) connectToHost: (NSString*)host;
- (void) reconnect;
- (void) disconnect;

- (void)setWill: (NSString *)payload toTopic:(NSString *)willTopic withQos:(NSUInteger)willQos retain:(BOOL)retain;
- (void)clearWill;

- (void)publishString: (NSString *)payload toTopic:(NSString *)topic withQos:(NSUInteger)qos retain:(BOOL)retain;

- (void)subscribe: (NSString *)topic;
- (void)subscribe: (NSString *)topic withQos:(NSUInteger)qos;
- (void)unsubscribe: (NSString *)topic;

@end
