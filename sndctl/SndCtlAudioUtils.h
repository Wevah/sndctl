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
 @param	error		The error, upon failure.
 @return The device's name, or \c NULL if an error occurred.
 */
CFStringRef SndCtlCopyNameOfDeviceID(AudioObjectID deviceid, CFErrorRef *error);

/**
 Get the number of channels of an audio device.
 @param	deviceid	The ID of the audio device.
 @param error		The error, upon failure.
 @return The number of channels. Returns \c 0 and sets \c error on failure.
 @discussion Usually returns \c 2\n.
 */
UInt32 SndCtlNumberOfChannelsOfDeviceID(AudioObjectID deviceid, CFErrorRef *error);

/// Dictionary key representing an attribute of an audio device.
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

 @param error	An optional \c CFErrorRef to be set if the function fails.
 @return An array of dictionaries represending the valid audio devices. Returns \c NULL
 	and sets \c error on failure.
 @discussion Valid returned audio devices currently include 2-channel devices.
 	Dictionary keys are listed under \c SndCtlAudioDeviceAttribute\n.
 */
CFArrayRef SndCtlCopyAudioOutputDevices(CFErrorRef *error);

bool SndCtlSetDefaultOutputDeviceID(AudioObjectID deviceid, CFErrorRef *error);

bool SndCtlSetVolume(AudioObjectID deviceid, Float32 volume, CFErrorRef *error);

bool SndCtlSetBalance(AudioObjectID deviceid, Float32 balance, CFErrorRef *error);

Float32 SndCtlGetVolume(AudioObjectID deviceid, CFErrorRef *error);

Float32 SndCtlGetBalance(AudioObjectID deviceid, CFErrorRef *error);

bool SndCtlIncrementVolume(AudioObjectID deviceid, Float32 delta, CFErrorRef *error);

bool SndCtlIncrementBalance(AudioObjectID deviceid, Float32 delta, CFErrorRef *error);

/**
 Returns the ID of the audio device whose prefix matches \c prefix\n.

 @param prefix The prefix to match, case-insensitively.
 @return The matched audio device ID, or \c kAudioDeviceUnknown the prefix does not match exactly one device name.
 */
AudioObjectID SndCtlAudioDeviceStartingWithString(char *prefix, CFErrorRef *error);

CFArrayRef SndCtlCopyAudioDevicesStartingWithString(const char *prefix, CFErrorRef *error);

#endif /* SndCtlAudioUtils_h */
