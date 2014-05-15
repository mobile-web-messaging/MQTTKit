//
//  MQTTKitTests.m
//  MQTTKitTests
//
//  Created by Jeff Mesnil on 22/10/2013.
//  Copyright (c) 2013 Jeff Mesnil. All rights reserved.
//

#import <XCTest/XCTest.h>
#import "MQTTKit.h"

#define secondsToNanoseconds(t) (t * 1000000000ull) // in nanoseconds
#define gotSignal(semaphore, timeout) ((dispatch_semaphore_wait(semaphore, dispatch_time(DISPATCH_TIME_NOW, secondsToNanoseconds(timeout)))) == 0l)

#define M2M 1

#if M2M

#define kHost @"m2m.eclipse.org"

#else

#define kHost @"localhost"

#endif

@interface MQTTKitTests : XCTestCase

@end

@implementation MQTTKitTests

MQTTClient *client;
NSString *topic;

- (void)setUp
{
    [super setUp];

    client = [[MQTTClient alloc] initWithClientId:[NSString stringWithFormat:@"MQTTKitTests-%@", [[NSUUID UUID] UUIDString]]];
    client.username = @"user";
    client.password = @"password";
    client.host = kHost;
    
    topic = [NSString stringWithFormat:@"MQTTKitTests/%@", [[NSUUID UUID] UUIDString]];
}

- (void)tearDown
{
    if (client.connected) {
        dispatch_semaphore_t disconnected = dispatch_semaphore_create(0);
        [client disconnectWithCompletionHandler:^(NSUInteger code) {
            dispatch_semaphore_signal(disconnected);
        }];
        XCTAssertTrue(gotSignal(disconnected, 4));
    }

#ifdef M2M
    //[self deleteTopic:topic];
#endif

    // Put teardown code here. This method is called after the invocation of each test method in the class.
    [super tearDown];
}

- (void)deleteTopic:(NSString *)topic
{
    NSMutableURLRequest *request = [[NSMutableURLRequest alloc] init];
    request.URL = [NSURL URLWithString:[NSString stringWithFormat:@"http://eclipse.mqttbridge.com/%@",
                                        [topic stringByReplacingOccurrencesOfString:@"/"
                                                                         withString:@"%2F"]]];
    request.HTTPMethod = @"DELETE";
    
    NSHTTPURLResponse *response;
    NSError *error;
    NSLog(@"DELETE %@", request.URL);
    [NSURLConnection sendSynchronousRequest:request returningResponse:&response error:&error];
    if (error) {
        XCTFail(@"%@", error);
    }
    XCTAssertEqual((NSInteger)200, response.statusCode);
}

- (void)testConnectOnUnknownServer
{
    dispatch_semaphore_t connectError = dispatch_semaphore_create(0);

    client.host = @"this.is.not.a.mqtt.server";

    [client connectWithCompletionHandler:^(MQTTConnectionReturnCode code) {
        if (code == ConnectionRefusedServerUnavailable) {
            dispatch_semaphore_signal(connectError);
        }
    }];

    XCTAssertFalse(gotSignal(connectError, 4));
    
    XCTAssertFalse(client.connected);
}

- (void)testConnectDisconnect
{
    dispatch_semaphore_t connected = dispatch_semaphore_create(0);
    
    client.reconnectDelay = 1;
    client.reconnectDelayMax = 10;
    client.reconnectExponentialBackoff = YES;
    
    [client connectWithCompletionHandler:^(MQTTConnectionReturnCode code) {
        if (code == ConnectionAccepted) {
            dispatch_semaphore_signal(connected);
        }
    }];

    XCTAssertTrue(gotSignal(connected, 4));
    XCTAssertTrue(client.connected);

    dispatch_semaphore_t disconnected = dispatch_semaphore_create(0);

    [client disconnectWithCompletionHandler:^(NSUInteger code) {
        dispatch_semaphore_signal(disconnected);
    }];

    XCTAssertTrue(gotSignal(disconnected, 4));
    XCTAssertFalse(client.connected);
}

- (void)testPublish
{
    dispatch_semaphore_t subscribed = dispatch_semaphore_create(0);

    [client connectWithCompletionHandler:^(NSUInteger code) {
        [client subscribe:topic
                  withQos:AtMostOnce
        completionHandler:^(NSArray *grantedQos) {
            for (NSNumber *qos in grantedQos) {
                NSLog(@"%@", qos);
            }
            dispatch_semaphore_signal(subscribed);
        }];
    }];

    XCTAssertTrue(gotSignal(subscribed, 4));

    NSString *text = [NSString stringWithFormat:@"Hello, MQTT %d", arc4random()];

    dispatch_semaphore_t received = dispatch_semaphore_create(0);

    [client setMessageHandler:^(MQTTMessage *message) {
        XCTAssertTrue([text isEqualToString:message.payloadString]);
        dispatch_semaphore_signal(received);
    }];

    dispatch_semaphore_t published = dispatch_semaphore_create(0);

    [client publishString:text toTopic:topic
                  withQos:AtMostOnce
                   retain:YES
        completionHandler:^(int mid) {
            dispatch_semaphore_signal(published);
    }];

    XCTAssertTrue(gotSignal(published, 4));

    XCTAssertTrue(gotSignal(received, 4));

    [client disconnectWithCompletionHandler:nil];
}

- (void)testPublishMany
{
    dispatch_semaphore_t subscribed = dispatch_semaphore_create(0);
    
    [client connectWithCompletionHandler:^(NSUInteger code) {
        [client subscribe:topic
                  withQos:AtMostOnce
        completionHandler:^(NSArray *grantedQos) {
            dispatch_semaphore_signal(subscribed);
        }];
    }];
    
    XCTAssertTrue(gotSignal(subscribed, 4));
    
    NSString *text = [NSString stringWithFormat:@"Hello, MQTT %d", arc4random()];
    
    int count = 10;
    for (int i = 0; i < count; i++) {
        [client publishString:text
                      toTopic:topic
                      withQos:AtMostOnce
                       retain:NO
            completionHandler:^(int mid) {
                NSLog(@"published message %i", mid);
        }];
    }
    
    dispatch_semaphore_t received = dispatch_semaphore_create(0);

    __block int receivedCount = 0;
    [client setMessageHandler:^(MQTTMessage *message) {
        NSLog(@"received message");
        XCTAssertTrue([text isEqualToString:message.payloadString]);
        receivedCount++;
        if (receivedCount == count) {
            dispatch_semaphore_signal(received);
        }
    }];
    
    XCTAssertTrue(gotSignal(received, 6));
    
    [client disconnectWithCompletionHandler:nil];
}

- (void)testTwoClients
{
    MQTTClient *subscriber = [[MQTTClient alloc] initWithClientId:@"MQTTKitTests-sub"];

    dispatch_semaphore_t subscribed = dispatch_semaphore_create(0);
    NSLog(@"connecting subscriber...");
    [subscriber connectToHost:kHost
            completionHandler:^(MQTTConnectionReturnCode code) {
                NSLog(@"subscriber connected");
                NSLog(@"subscriber subscribing...");
                [subscriber subscribe:topic
                              withQos:AtMostOnce
                    completionHandler:^(NSArray *grantedQos) {
                        NSLog(@"subscriber subscribed");
                        dispatch_semaphore_signal(subscribed);
                    }];
            }];
    XCTAssertTrue(gotSignal(subscribed, 4));

    NSString *text = [NSString stringWithFormat:@"Hello, MQTT %d", arc4random()];
    dispatch_semaphore_t received = dispatch_semaphore_create(0);
    subscriber.messageHandler = ^(MQTTMessage *message) {
        XCTAssertTrue([text isEqualToString:message.payloadString]);
        dispatch_semaphore_signal(received);
    };

    MQTTClient *publisher = [[MQTTClient alloc] initWithClientId:@"MQTTKitTests-pub"];
    dispatch_semaphore_t published = dispatch_semaphore_create(0);
    [publisher connectToHost:kHost
           completionHandler:^(MQTTConnectionReturnCode code) {
               [publisher publishString:text toTopic:topic
                                withQos:AtMostOnce
                                 retain:YES
                      completionHandler:^(int mid) {
                          dispatch_semaphore_signal(published);
                      }];
           }];
    XCTAssertTrue(gotSignal(published, 4));

    XCTAssertTrue(gotSignal(received, 4));

    [publisher disconnectWithCompletionHandler:nil];
    [subscriber disconnectWithCompletionHandler:nil];
}

- (void)testUnsubscribe
{
    dispatch_semaphore_t subscribed = dispatch_semaphore_create(0);

    [client connectWithCompletionHandler:^(NSUInteger code) {
        [client subscribe:topic
                  withQos:AtMostOnce
        completionHandler:^(NSArray *grantedQos) {
             dispatch_semaphore_signal(subscribed);
         }];
    }];

    XCTAssertTrue(gotSignal(subscribed, 4));

    NSString *text = [NSString stringWithFormat:@"Hello, MQTT %d", arc4random()];

    dispatch_semaphore_t received = dispatch_semaphore_create(0);
    
    [client setMessageHandler:^(MQTTMessage *message) {
        XCTAssertTrue([text isEqualToString:message.payloadString]);
        dispatch_semaphore_signal(received);
    }];
    
    dispatch_semaphore_t unsubscribed = dispatch_semaphore_create(0);

    [client unsubscribe:topic withCompletionHandler:^{
        dispatch_semaphore_signal(unsubscribed);
    }];

    XCTAssertTrue(gotSignal(unsubscribed, 4));

    dispatch_semaphore_t published = dispatch_semaphore_create(0);

    [client publishString:text
                  toTopic:topic
                  withQos:AtMostOnce
                   retain:NO
        completionHandler:^(int mid) {
        dispatch_semaphore_signal(published);
    }];

    XCTAssertTrue(gotSignal(published, 4));

    XCTAssertFalse(gotSignal(received, 2));

    [client disconnectWithCompletionHandler:nil];
}

- (void)testCleanSession
{
    MQTTClient *cleanSessionClient = [[MQTTClient alloc] initWithClientId:@"MQTTKitTests-cleanSession2"
                                                             cleanSession:NO];

    dispatch_semaphore_t subscribed = dispatch_semaphore_create(0);
    [cleanSessionClient connectToHost:kHost
                    completionHandler:^(MQTTConnectionReturnCode code) {
                        [cleanSessionClient subscribe:topic
                                              withQos:AtLeastOnce
                                    completionHandler:^(NSArray *grantedQos) {
                                        dispatch_semaphore_signal(subscribed);
                                    }];
                    }];
    XCTAssertTrue(gotSignal(subscribed, 4));

    dispatch_semaphore_t disconnected = dispatch_semaphore_create(0);
    [cleanSessionClient disconnectWithCompletionHandler:^(NSUInteger code) {
        dispatch_semaphore_signal(disconnected);
    }];
    XCTAssertTrue(gotSignal(disconnected, 4));

    NSString *text = [NSString stringWithFormat:@"Hello, MQTT for clean session %d", arc4random()];

    cleanSessionClient = [[MQTTClient alloc] initWithClientId:@"MQTTKitTests-cleanSession2"
                                                 cleanSession:NO];
    dispatch_semaphore_t received = dispatch_semaphore_create(0);
    cleanSessionClient.messageHandler = ^(MQTTMessage *message) {
        XCTAssertTrue([text isEqualToString:message.payloadString]);
        dispatch_semaphore_signal(received);
    };

    XCTAssertFalse(gotSignal(received, 4));

    dispatch_semaphore_t published = dispatch_semaphore_create(0);
    [client connectToHost:kHost
        completionHandler:^(MQTTConnectionReturnCode code) {
            [client publishString:text
                          toTopic:topic
                          withQos:AtLeastOnce
                           retain:NO
                completionHandler:^(int mid) {
                    dispatch_semaphore_signal(published);
                }];
            [client disconnectWithCompletionHandler:nil];
        }];

    XCTAssertTrue(gotSignal(published, 8));

    dispatch_semaphore_t connected2 = dispatch_semaphore_create(0);
    [cleanSessionClient connectToHost:kHost
                    completionHandler:^(MQTTConnectionReturnCode code) {
                        dispatch_semaphore_signal(connected2);
                    }];

    XCTAssertTrue(gotSignal(connected2, 4));
    XCTAssertTrue(gotSignal(received, 4));

    disconnected = dispatch_semaphore_create(0);
    [cleanSessionClient disconnectWithCompletionHandler:^(NSUInteger code) {
        dispatch_semaphore_signal(disconnected);
    }];
    XCTAssertTrue(gotSignal(disconnected, 4));
}

@end
