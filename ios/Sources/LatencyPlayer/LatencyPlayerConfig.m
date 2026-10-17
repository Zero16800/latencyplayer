//
//  LatencyPlayerConfig.m
//

#import "LatencyPlayerConfig.h"

const int LatencyPlayerDefaultBufferTime = 200;
const int LatencyPlayerDefaultReconnectInterval = 3;

@implementation LatencyPlayerConfig

+ (instancetype)config
{
    return [[self alloc] init];
}

+ (instancetype)configWithURL:(NSString *)url
{
    LatencyPlayerConfig *c = [[self alloc] init];
    c.url = url;
    return c;
}

+ (instancetype)build:(void (^)(LatencyPlayerConfig *))block
{
    LatencyPlayerConfig *c = [[self alloc] init];
    if (block) {
        block(c);
    }
    return c;
}

- (instancetype)init
{
    self = [super init];
    if (self) {
        _bufferTime = LatencyPlayerDefaultBufferTime;
        _fastStartup = YES;
        _lowLatency = NO;
        _hardwareDecoder = YES;
        _autoReconnect = YES;
        _reconnectInterval = LatencyPlayerDefaultReconnectInterval;
        _maxReconnectCount = 0;
        _orientation = 1;
        _mute = NO;
        _volume = 100;
    }
    return self;
}

- (void)setBufferTime:(int)bufferTime
{
    _bufferTime = MAX(0, MIN(5000, bufferTime));
}

- (void)setVolume:(int)volume
{
    _volume = MAX(0, MIN(100, volume));
}

#pragma mark - NSCopying

- (id)copyWithZone:(NSZone *)zone
{
    LatencyPlayerConfig *c = [[[self class] allocWithZone:zone] init];
    c.url = self.url;
    c.bufferTime = self.bufferTime;
    c.fastStartup = self.fastStartup;
    c.lowLatency = self.lowLatency;
    c.hardwareDecoder = self.hardwareDecoder;
    c.autoReconnect = self.autoReconnect;
    c.reconnectInterval = self.reconnectInterval;
    c.maxReconnectCount = self.maxReconnectCount;
    c.orientation = self.orientation;
    c.mute = self.mute;
    c.volume = self.volume;
    return c;
}

@end
