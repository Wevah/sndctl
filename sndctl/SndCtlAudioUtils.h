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
 Key representing the name of an audio device.
 @discussion Value is a \c CFStringRef\n.
 */
extern const SndCtlAudioDeviceAttribute kSndCtlAudioDeviceAttributeName;

/**
 Copies an array of audio output devices.

 @param error	An optional \c CFErrorRef to be set if the function fails.
 @return An array of dictionaries represending the valid audio devices. Returns \c NULL
 	and sets \c error on failure.
 @discussion Valid returned audio devices currently include 2-channel devices.
 	See \c SndCtlAudioDeviceAttribute for valid keys.
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
 Returns an array of audio devices whose name matches a string.

 @param		stringToMatch	The string to match, case-insensitively.
 @param		error			An error on failure.
 @return	An array of dictionaries representing the matched audio devices.
 			See \c SndCtlAudioDeviceAttribute for valid keys.
 */
CFArrayRef SndCtlCopyAudioDevicesMatchingString(const char *stringToMatch, CFErrorRef *error);

#endif /* SndCtlAudioUtils_h */
