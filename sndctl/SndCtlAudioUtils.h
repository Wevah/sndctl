//
//  SndCtlAudioUtils.h
//  sndctl
//
//  Created by Nate Weaver on 2019-01-27.
//  Copyright © 2019 Nate Weaver/Derailer. All rights reserved.
//

#ifndef SndCtlAudioUtils_h
#define SndCtlAudioUtils_h

#include <stdio.h>
#include <AudioToolbox/AudioToolbox.h>

CFStringRef SndCtlCopyNameOfDeviceID(AudioObjectID devid);

UInt32 SndCtlNumberOfChannelsOfDeviceID(AudioObjectID devid);

CFArrayRef SndCtlCopyAudioOutputDevices(void);

AudioObjectID SndCtlDefaultOutputDeviceID(void);

void SndCtlSetDefaultOutputDeviceID(AudioObjectID deviceID);

char *SndCtlNameForDeviceProperty(AudioObjectPropertySelector selector);

OSStatus SndCtlGetOutputDeviceFloatProperty(AudioObjectID devid, AudioObjectPropertySelector selector, Float32 *value);

OSStatus SndCtlSetDeviceFloatProperty(AudioObjectID devid, AudioObjectPropertySelector selector, Float32 value);

OSStatus SndCtlSetVolume(AudioObjectID devid, Float32 volume);

OSStatus SndCtlSetBalance(AudioObjectID devid, Float32 balance);

OSStatus SndCtlGetCurrentVolume(AudioObjectID devid, Float32 *volume);

OSStatus SndCtlGetCurrentBalance(AudioObjectID devid, Float32 *volume);

OSStatus SndCtlIncrementVolume(AudioObjectID devid, Float32 delta);

OSStatus SndCtlIncrementBalance(AudioObjectID devid, Float32 delta);

/**
 Returns the ID of the audio device whose prefix matches \c prefix\n.

 @param prefix The prefix to match, case-insensitively.
 @return The matched audio device ID, or kAudioDeviceUnknown the prefix does not match exactly one device name.
 */
AudioObjectID SndCtlAudioDeviceStartingWithString(char *prefix);

void SndCtlPrintInfoForError(AudioObjectID devid, AudioObjectPropertySelector selector, OSStatus result, bool isSetter);

#endif /* SndCtlAudioUtils_h */
