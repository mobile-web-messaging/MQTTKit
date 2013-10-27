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
NSString *topic;

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
    client.host = kHost;
    
    topic = [NSString stringWithFormat:@"MQTTKitTests/%@", [[NSUUID UUID] UUIDString]];
}

- (void)tearDown
{
    [client disconnect];

#ifdef M2M
    [self deleteTopic:topic];
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

- (void)testConnect
{
    [client connect];

    XCTAssertTrue(gotSignal(self.connected, 4));

    [client disconnect];

    XCTAssertTrue(gotSignal(self.disconnected, 4));

    [client disconnect];
}

- (void)testPublish
{
    [client connect];

    XCTAssertTrue(gotSignal(self.connected, 4));

    [client subscribe:topic withQos:0];

    XCTAssertTrue(gotSignal(self.subscribed, 4));

    NSString *text = @"Hello, MQTT";
    [client publishString:text toTopic:topic withQos:0 retain:YES];

    XCTAssertTrue(gotSignal(self.published, 4));

    XCTAssertTrue(gotSignal(self.received, 4));
    NSLog(@"message = %@", message.payload);
    XCTAssertTrue([text isEqualToString:message.payload]);

    [client disconnect];
    XCTAssertTrue(gotSignal(self.disconnected, 4));
}

- (void)testUnsubscribe
{
    [client connect];

    XCTAssertTrue(gotSignal(self.connected, 4));

    [client subscribe:topic withQos:0];

    XCTAssertTrue(gotSignal(self.subscribed, 4));

    [client unsubscribe:topic];

    XCTAssertTrue(gotSignal(self.unsubscribed, 4));

    NSString *text = @"Hello, MQTT";
    [client publishString:text toTopic:topic withQos:0 retain:NO];

    XCTAssertTrue(gotSignal(self.published, 4));

    XCTAssertFalse(gotSignal(self.received, 2));

    [client disconnect];
    XCTAssertTrue(gotSignal(self.disconnected, 4));
}

#pragma mark MQTTClientDelegate

- (void) client:(MQTTClient *)client didConnect: (NSUInteger)code
{
    NSLog(@"%s", __PRETTY_FUNCTION__);
    dispatch_semaphore_signal(self.connected);
}

- (void) client:(MQTTClient *)client didDisconnect: (NSUInteger)code
{
    NSLog(@"%s", __PRETTY_FUNCTION__);
    dispatch_semaphore_signal(self.disconnected);
}

- (void) client:(MQTTClient *)client didPublish: (NSUInteger)messageID
{
    NSLog(@"%s", __PRETTY_FUNCTION__);
    dispatch_semaphore_signal(self.published);
}

- (void) client:(MQTTClient *)client didReceiveMessage: (MQTTMessage*)aMessage {
    NSLog(@"%s", __PRETTY_FUNCTION__);
    dispatch_semaphore_signal(self.received);
    self.message = aMessage;
}

- (void) client:(MQTTClient *)client didSubscribe: (NSUInteger)messageID grantedQos:(NSArray*)qos
{
    NSLog(@"%s", __PRETTY_FUNCTION__);
    dispatch_semaphore_signal(self.subscribed);
}

- (void) client:(MQTTClient *)client didUnsubscribe: (NSUInteger)messageID
{
    NSLog(@"%s", __PRETTY_FUNCTION__);
    dispatch_semaphore_signal(self.unsubscribed);
}

@end
