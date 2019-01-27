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


/**
 Copy the name of an audio device.
 @param deviceid	The ID of the audio device.
 @return The device's name, or \c NULL if an error occurred.
 The caller is responsible for releasing the returned value.
 */
CFStringRef SndCtlCopyNameOfDeviceID(AudioObjectID deviceid);

/**
 Get the number of channels of an audio device.
 @return The number of channels.
 @discussion Usually returns \c 2\n.
 */
UInt32 SndCtlNumberOfChannelsOfDeviceID(AudioObjectID devid);

/// Key representing an attribute of an audio device.
typedef CFStringRef SndCtlAudioDeviceAttribute;

/**
 Key representing the ID of an audio device.
 @discussion Value is a \c CFNumber wrapping an \c Int32\n.
 */
extern const SndCtlAudioDeviceAttribute kSndCtlAudioDeviceAttributeID;

/**
 Key representing the name of an audio device. Value is a \c CFStringRef\n.
 @discussion Value is a \c CFStringRef\n.
 */
extern const SndCtlAudioDeviceAttribute kSndCtlAudioDeviceAttributeName;

/**
 Copies an array of audio output devices.

 @return An array of dictionaries represending the valid audio devices.
 @discussion Valid returned audio devices currently include 2-channel devices.
 	Dictionary keys are \c CFSTR("id") and \c CFSTR("name")\n.
 */
CFArrayRef SndCtlCopyAudioOutputDevices(void);

/**
 Get the ID of the current default audio output device.
 @return The ID of the default device.
 */
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
