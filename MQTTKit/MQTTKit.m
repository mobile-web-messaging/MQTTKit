//
//  MQTTKit.m
//  MQTTKit
//
//  Created by Jeff Mesnil on 22/10/2013.
//  Copyright (c) 2013 Jeff Mesnil. All rights reserved.
//  Copyright 2012 Nicholas Humfrey. All rights reserved.
//

#import "MQTTKit.h"
#import "mosquitto.h"

@implementation MQTTMessage

@synthesize mid, topic, payload, payloadlen, qos, retained;

-(id)init
{
    self.mid = 0;
    self.topic = nil;
    self.payload = nil;
    self.payloadlen = 0;
    self.qos = 0;
    self.retained = FALSE;
    return self;
}

@end

@interface MQTTClient()

@property (nonatomic, assign) BOOL connected;

@end

@implementation MQTTClient

@synthesize host;
@synthesize port;
@synthesize username;
@synthesize password;
@synthesize keepAlive;
@synthesize cleanSession;
@synthesize delegate;
@synthesize connected;


static void on_connect(struct mosquitto *mosq, void *obj, int rc)
{
    MQTTClient* client = (__bridge MQTTClient*)obj;
    NSLog(@"on_connect rc = %d", rc);
    client.connected = YES;
    [client.delegate client:client didConnect:rc];
}

static void on_disconnect(struct mosquitto *mosq, void *obj, int rc)
{
    MQTTClient* client = (__bridge MQTTClient*)obj;
    NSLog(@"on_disconnect rc = %d", rc);
    client.connected = NO;
    [client.delegate client:client didDisconnect:rc];
}

static void on_publish(struct mosquitto *mosq, void *obj, int message_id)
{
    MQTTClient* client = (__bridge MQTTClient*)obj;
    [client.delegate client:client didPublish:message_id];
}

static void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *mosq_msg)
{
    MQTTMessage *message = [[MQTTMessage alloc] init];
    message.topic = [NSString stringWithUTF8String: mosq_msg->topic];
    message.payload = [[NSString alloc] initWithBytes:mosq_msg->payload
                                                 length:mosq_msg->payloadlen
                                               encoding:NSUTF8StringEncoding];
    MQTTClient* client = (__bridge MQTTClient*)obj;
    [client.delegate client:client didReceiveMessage:message];
}

static void on_subscribe(struct mosquitto *mosq, void *obj, int message_id, int qos_count, const int *granted_qos)
{
    MQTTClient* client = (__bridge MQTTClient*)obj;
    [client.delegate client:client didSubscribe:message_id grantedQos:nil];
}

static void on_unsubscribe(struct mosquitto *mosq, void *obj, int message_id)
{
    MQTTClient* client = (__bridge MQTTClient*)obj;
    [client.delegate client:client didUnsubscribe:message_id];
}


// Initialize is called just before the first object is allocated
+ (void)initialize {
    mosquitto_lib_init();
}

+ (NSString*)version {
    int major, minor, revision;
    mosquitto_lib_version(&major, &minor, &revision);
    return [NSString stringWithFormat:@"%d.%d.%d", major, minor, revision];
}

- (MQTTClient*) initWithClientId: (NSString*) clientId {
    if ((self = [super init])) {
        const char* cstrClientId = [clientId cStringUsingEncoding:NSUTF8StringEncoding];
        [self setHost: nil];
        [self setPort: 1883];
        [self setKeepAlive: 60];
        [self setCleanSession: YES]; //NOTE: this isdisable clean to keep the broker remember this client

        mosq = mosquitto_new(cstrClientId, cleanSession, (__bridge void *)(self));
        mosquitto_connect_callback_set(mosq, on_connect);
        mosquitto_disconnect_callback_set(mosq, on_disconnect);
        mosquitto_publish_callback_set(mosq, on_publish);
        mosquitto_message_callback_set(mosq, on_message);
        mosquitto_subscribe_callback_set(mosq, on_subscribe);
        mosquitto_unsubscribe_callback_set(mosq, on_unsubscribe);
        timer = nil;
    }
    return self;
}


- (void) connect {
    const char *cstrHost = [host cStringUsingEncoding:NSASCIIStringEncoding];
    const char *cstrUsername = NULL, *cstrPassword = NULL;

    if (username)
        cstrUsername = [username cStringUsingEncoding:NSUTF8StringEncoding];

    if (password)
        cstrPassword = [password cStringUsingEncoding:NSUTF8StringEncoding];

    // FIXME: check for errors
    mosquitto_username_pw_set(mosq, cstrUsername, cstrPassword);

    mosquitto_connect(mosq, cstrHost, port, keepAlive);

    // Setup timer to handle network events
    // FIXME: better way to do this - hook into iOS Run Loop select() ?
    // or run in seperate thread?
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        NSLog(@"start loop");
        mosquitto_loop_forever(mosq, 10, 1);
        NSLog(@"end loop");
    });

    connected = YES;
}

- (void) connectToHost: (NSString*)aHost {
    [self setHost:aHost];
    [self connect];
}

- (void) reconnect {
    mosquitto_reconnect(mosq);
}

- (void) disconnect {
    mosquitto_disconnect(mosq);
}

- (void)setWill: (NSString *)payload toTopic:(NSString *)willTopic withQos:(NSUInteger)willQos retain:(BOOL)retain;
{
    const char* cstrTopic = [willTopic cStringUsingEncoding:NSUTF8StringEncoding];
    const uint8_t* cstrPayload = (const uint8_t*)[payload cStringUsingEncoding:NSUTF8StringEncoding];
    size_t cstrlen = [payload lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
    mosquitto_will_set(mosq, cstrTopic, cstrlen, cstrPayload, willQos, retain);
}


- (void)clearWill
{
    mosquitto_will_clear(mosq);
}


- (void)publishString: (NSString *)payload toTopic:(NSString *)topic withQos:(NSUInteger)qos retain:(BOOL)retain {
    const char* cstrTopic = [topic cStringUsingEncoding:NSUTF8StringEncoding];
    const uint8_t* cstrPayload = (const uint8_t*)[payload cStringUsingEncoding:NSUTF8StringEncoding];
    size_t cstrlen = [payload lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
    mosquitto_publish(mosq, NULL, cstrTopic, cstrlen, cstrPayload, qos, retain);

}



- (void)subscribe: (NSString *)topic {
    [self subscribe:topic withQos:0];
}

- (void)subscribe: (NSString *)topic withQos:(NSUInteger)qos {
    const char* cstrTopic = [topic cStringUsingEncoding:NSUTF8StringEncoding];
    mosquitto_subscribe(mosq, NULL, cstrTopic, qos);
}

- (void)unsubscribe: (NSString *)topic {
    const char* cstrTopic = [topic cStringUsingEncoding:NSUTF8StringEncoding];
    mosquitto_unsubscribe(mosq, NULL, cstrTopic);
}


- (void) setMessageRetry: (NSUInteger)seconds
{
    mosquitto_message_retry_set(mosq, (unsigned int)seconds);
}

- (void) dealloc {
    if (mosq) {
        mosquitto_destroy(mosq);
        mosq = NULL;
    }

    if (timer) {
        [timer invalidate];
        timer = nil;
    }
}

@end
