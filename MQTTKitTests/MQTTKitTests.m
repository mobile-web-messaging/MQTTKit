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

@interface MQTTKitTests : XCTestCase<MQTTClientDelegate>

@property(nonatomic, strong) dispatch_semaphore_t connected;
@property(nonatomic, strong) dispatch_semaphore_t disconnected;
@property(nonatomic, strong) dispatch_semaphore_t subscribed;
@property(nonatomic, strong) dispatch_semaphore_t unsubscribed;
@property(nonatomic, strong) dispatch_semaphore_t published;
@property(nonatomic, strong) dispatch_semaphore_t received;
@property(nonatomic, strong) MQTTMessage *message;

@end

@implementation MQTTKitTests

@synthesize connected, disconnected, subscribed, unsubscribed, published, received;
@synthesize message;

MQTTClient *client;

- (void)setUp
{
    [super setUp];
    // Put setup code here. This method is called before the invocation of each test method in the class.
    self.connected = dispatch_semaphore_create(0);
    self.disconnected = dispatch_semaphore_create(0);
    self.subscribed = dispatch_semaphore_create(0);
    self.unsubscribed = dispatch_semaphore_create(0);
    self.published = dispatch_semaphore_create(0);
    self.received = dispatch_semaphore_create(0);

    client = [[MQTTClient alloc] initWithClientId:@"MQTTKitTests"];
    client.delegate = self;
    client.username = @"user";
    client.password = @"password";
    client.host = @"localhost";
}

- (void)tearDown
{
    [client disconnect];
    // Put teardown code here. This method is called after the invocation of each test method in the class.
    [super tearDown];
}

- (void)testConnect
{
    [client connect];

    XCTAssertTrue(gotSignal(self.connected, 2));

    [client disconnect];

    XCTAssertTrue(gotSignal(self.disconnected, 2));

    [client disconnect];
}

- (void)testPublish
{
    [client connect];

    XCTAssertTrue(gotSignal(self.connected, 2));

    NSString *topic = @"test/testPublish";
    [client subscribe:topic withQos:0];

    XCTAssertTrue(gotSignal(self.subscribed, 2));

    NSString *text = @"Hello, MQTT";
    [client publishString:text toTopic:topic withQos:0 retain:YES];

    XCTAssertTrue(gotSignal(self.published, 2));

    XCTAssertTrue(gotSignal(self.received, 2));
    NSLog(@"message = %@", message.payload);
    XCTAssertTrue([text isEqualToString:message.payload]);

    [client disconnect];
    XCTAssertTrue(gotSignal(self.disconnected, 2));
}

- (void)testUnsubscribe
{
    [client connect];

    XCTAssertTrue(gotSignal(self.connected, 2));

    NSString *topic = @"test/testPublish";
    [client subscribe:topic withQos:0];

    XCTAssertTrue(gotSignal(self.subscribed, 2));

    [client unsubscribe:topic];

    XCTAssertTrue(gotSignal(self.unsubscribed, 2));

    NSString *text = @"Hello, MQTT";
    [client publishString:text toTopic:topic withQos:0 retain:YES];

    XCTAssertTrue(gotSignal(self.published, 2));

    XCTAssertFalse(gotSignal(self.received, 1));

    [client disconnect];
    XCTAssertTrue(gotSignal(self.disconnected, 2));
}

#pragma mark MQTTClientDelegate

- (void) didConnect: (NSUInteger)code {
    NSLog(@"%s", __PRETTY_FUNCTION__);
    dispatch_semaphore_signal(self.connected);
}

- (void) didDisconnect {
    NSLog(@"%s", __PRETTY_FUNCTION__);
    dispatch_semaphore_signal(self.disconnected);
}

- (void) didPublish: (NSUInteger)messageId {
    NSLog(@"%s", __PRETTY_FUNCTION__);
    dispatch_semaphore_signal(self.published);
}

- (void) didReceiveMessage: (MQTTMessage*)mosq_msg {
    NSLog(@"%s", __PRETTY_FUNCTION__);
    dispatch_semaphore_signal(self.received);
    self.message = mosq_msg;
}

- (void) didSubscribe: (NSUInteger)messageId grantedQos:(NSArray*)qos {
    NSLog(@"%s", __PRETTY_FUNCTION__);
    dispatch_semaphore_signal(self.subscribed);
}

- (void) didUnsubscribe: (NSUInteger)messageId {
    NSLog(@"%s", __PRETTY_FUNCTION__);
    dispatch_semaphore_signal(self.unsubscribed);
}

@end
