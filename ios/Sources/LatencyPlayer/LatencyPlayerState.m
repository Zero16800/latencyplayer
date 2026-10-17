//
//  LatencyPlayerState.m
//

#import "LatencyPlayerState.h"

LatencyPlayerState LatencyPlayerStateFromValue(NSInteger value)
{
    if (value < 0 || value > 8) {
        return LatencyPlayerStateIdle;
    }
    return (LatencyPlayerState)value;   // contiguous 0..8, matches the enum
}

NSString *LatencyPlayerStateToString(LatencyPlayerState state)
{
    switch (state) {
        case LatencyPlayerStateIdle:         return @"IDLE";
        case LatencyPlayerStateInitializing: return @"INITIALIZING";
        case LatencyPlayerStateReady:        return @"READY";
        case LatencyPlayerStateBuffering:    return @"BUFFERING";
        case LatencyPlayerStatePlaying:      return @"PLAYING";
        case LatencyPlayerStatePaused:       return @"PAUSED";
        case LatencyPlayerStateStopping:     return @"STOPPING";
        case LatencyPlayerStateStopped:      return @"STOPPED";
        case LatencyPlayerStateError:        return @"ERROR";
    }
    return @"IDLE";
}
