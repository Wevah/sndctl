//
//  SndCtlAudioUtils.c
//  sndctl
//
//  Created by Nate Weaver on 2019-01-27.
//  Copyright © 2019 Nate Weaver/Derailer. All rights reserved.
//

#include "SndCtlAudioUtils.h"

const SndCtlAudioDeviceAttribute kSndCtlAudioDeviceAttributeID = CFSTR("id");
const SndCtlAudioDeviceAttribute kSndCtlAudioDeviceAttributeName = CFSTR("name");
const SndCtlAudioDeviceAttribute kSndCtlAudioDeviceAttributeHasMainVolume = CFSTR("hasMainVolume");
const SndCtlAudioDeviceAttribute kSndCtlAudioDeviceAttributeHasMainBalance = CFSTR("hasMainBalance");

static CFErrorRef SndCtlErrorCreateWithOSStatus(OSStatus status, CFStringRef localizedFailure) {
	CFStringRef failureReason = NULL;

	switch (status) {
		case kAudioHardwareBadObjectError:
		case kAudioHardwareBadDeviceError:
			failureReason = CFSTR("Device doesn't exist.");
			break;
		case kAudioHardwareUnknownPropertyError:
			failureReason = CFSTR("Device doesn't support the specified property.");
		default:
			break;
	}

	if (failureReason)
		localizedFailure = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@: %@"), localizedFailure, failureReason);

	CFTypeRef keys[] = { kCFErrorLocalizedDescriptionKey, kCFErrorLocalizedFailureReasonKey };
	CFTypeRef values[] = { localizedFailure, failureReason };
	CFErrorRef error = CFErrorCreateWithUserInfoKeysAndValues(kCFAllocatorDefault, kCFErrorDomainOSStatus, status, keys, values, failureReason ? 2 : 1);

	if (failureReason)
		CFRelease(localizedFailure);

	return error;
}

CFStringRef SndCtlCopyNameOfDeviceID(AudioObjectID deviceid, CFErrorRef *error) {
	AudioObjectPropertyAddress theAddress = {
		kAudioObjectPropertyName,
		kAudioObjectPropertyScopeOutput,
		0
	};

	CFStringRef name;
	UInt32 maxlen = sizeof(name);

	OSStatus result = AudioObjectGetPropertyData(deviceid, &theAddress, 0, NULL, &maxlen, &name);

	if (result != kAudioHardwareNoError) {
		if (error) {
			CFStringRef localizedFailure = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("Couldn't get name of device with ID %u"), deviceid);
			*error = SndCtlErrorCreateWithOSStatus(result, localizedFailure);
			CFRelease(localizedFailure);
		}

		return NULL;
	}

	return name;
}

UInt32 SndCtlNumberOfChannelsOfDeviceID(AudioObjectID deviceid, CFErrorRef *error) {
	AudioObjectPropertyAddress theAddress = {
		kAudioDevicePropertyStreamConfiguration,
		kAudioDevicePropertyScopeOutput,
		0
	};

	UInt32 propSize;
	UInt32 numberOfChannels = 0;

	OSStatus result = AudioObjectGetPropertyDataSize(deviceid, &theAddress, 0, NULL, &propSize);

	if (result != kAudioHardwareNoError) {
		if (error) {
			CFStringRef localizedFailure = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("Couldn't get channels of device with ID %u"), deviceid);
			*error =  SndCtlErrorCreateWithOSStatus(result, localizedFailure);
			CFRelease(localizedFailure);
		}
		return 0;
	}

	AudioBufferList *buflist = (AudioBufferList *)malloc(propSize);

	result = AudioObjectGetPropertyData(deviceid, &theAddress, 0, NULL, &propSize, buflist);

	if (result == kAudioHardwareNoError) {
		for (UInt32 i = 0; i < buflist->mNumberBuffers; ++i)
			numberOfChannels += buflist->mBuffers[i].mNumberChannels;
	} else if (error) {
		CFStringRef localizedFailure = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("Couldn't get channels of device with ID %u"), deviceid);
		*error =  SndCtlErrorCreateWithOSStatus(result, localizedFailure);
		CFRelease(localizedFailure);
	}

	free(buflist);
	return numberOfChannels;
}

AudioObjectID *SndCtlGetAudioOutputDeviceIDs(CFErrorRef *error) {
	static AudioObjectID deviceids[64];
	UInt32 propsize;

	AudioObjectPropertyAddress theAddress = {
		kAudioHardwarePropertyDevices,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster
	};

	OSStatus result = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &theAddress, 0, NULL, &propsize);

	if (result != kAudioHardwareNoError) {
		if (error)
			*error = SndCtlErrorCreateWithOSStatus(result, CFSTR("Couldn't copy audio output devices."));

		return NULL;
	}

	int deviceCount = propsize / sizeof(AudioObjectID);
	if (deviceCount > 63)
		return NULL;

	AudioObjectID allDevices[deviceCount];
	result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &theAddress, 0, NULL, &propsize, allDevices);

	if (result != kAudioHardwareNoError) {
		if (error)
			*error = SndCtlErrorCreateWithOSStatus(result, CFSTR("Couldn't copy audio output devices."));

		return NULL;
	}

	int j = 0;
	for (int i = 0; i < deviceCount; ++i) {
		AudioObjectID deviceid = allDevices[i];
		if (SndCtlNumberOfChannelsOfDeviceID(deviceid, NULL) > 0) {
			deviceids[j++] = deviceid;
		}
	}

	deviceids[j] = kAudioObjectUnknown;

	return deviceids;
}

CFArrayRef SndCtlCopyAudioOutputDevices(CFErrorRef *error) {
	AudioObjectID *deviceids = SndCtlGetAudioOutputDeviceIDs(error);

	if (!deviceids)
		return NULL;

	CFMutableArrayRef devices = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
	AudioObjectID deviceid = kAudioObjectUnknown;

	while ( (deviceid = *deviceids++) ) {
		CFStringRef name = SndCtlCopyNameOfDeviceID(deviceid, NULL);
		CFBooleanRef hasVolume = SndCtlOutputDeviceHasMainVolume(deviceid) ? kCFBooleanTrue : kCFBooleanFalse;
		CFBooleanRef hasBalance = SndCtlOutputDeviceHasMainBalance(deviceid) ? kCFBooleanTrue : kCFBooleanFalse;

		CFTypeRef keys[] = { kSndCtlAudioDeviceAttributeID, kSndCtlAudioDeviceAttributeName, kSndCtlAudioDeviceAttributeHasMainVolume, kSndCtlAudioDeviceAttributeHasMainBalance };
		CFNumberRef idNumber = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &deviceid);
		CFTypeRef values[] = { idNumber, name, hasVolume, hasBalance };

		CFDictionaryRef device = CFDictionaryCreate(kCFAllocatorDefault, keys, values, sizeof(keys) / sizeof(keys[0]), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		CFArrayAppendValue(devices, device);

		CFRelease(idNumber);
		CFRelease(name);
	}

	return devices;
}

AudioObjectID SndCtlDefaultOutputDeviceID(CFErrorRef *error) {
	AudioObjectPropertyAddress defaultOutputDevicePropertyAddress = {
		kAudioHardwarePropertyDefaultOutputDevice,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster
	};

	AudioObjectID defaultOutputDeviceID;
	UInt32 deviceIDSize = sizeof(defaultOutputDeviceID);
	OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &defaultOutputDevicePropertyAddress, 0, NULL, &deviceIDSize, &defaultOutputDeviceID);

	if (result != kAudioHardwareNoError) {
		if (error) {
			*error = SndCtlErrorCreateWithOSStatus(result, CFSTR("Couldn't get default output device."));
		}
		return kAudioDeviceUnknown;
	}

	return defaultOutputDeviceID;
}

bool SndCtlSetDefaultOutputDeviceID(AudioObjectID deviceid, CFErrorRef *error) {
	AudioObjectPropertyAddress defaultOutputDevicePropertyAddress = {
		kAudioHardwarePropertyDefaultOutputDevice,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster
	};

	UInt32 deviceIDSize = sizeof(deviceid);
	OSStatus result = AudioObjectSetPropertyData(kAudioObjectSystemObject, &defaultOutputDevicePropertyAddress, 0, NULL, deviceIDSize, &deviceid);

	if (result != kAudioHardwareNoError) {
		if (error) {
			*error = SndCtlErrorCreateWithOSStatus(result, CFSTR("Couldn't set default output device."));
		}

		return false;
	}

	return true;
}

char *SndCtlNameForDeviceProperty(AudioObjectPropertySelector selector) {
	if (selector == kAudioHardwareServiceDeviceProperty_VirtualMainBalance)
		return "balance";
	else if (selector == kAudioHardwareServiceDeviceProperty_VirtualMainBalance)
		return "volume";

	return NULL;
}

static bool SndCtlOutputDeviceHasProperty(AudioObjectID deviceid, AudioObjectPropertySelector selector) {
	if (deviceid == kAudioDeviceUnknown)
		deviceid = SndCtlDefaultOutputDeviceID(NULL);
	if (deviceid == kAudioDeviceUnknown)
		return false;

	AudioObjectPropertyAddress propertyAddress = {
		selector,
		kAudioObjectPropertyScopeOutput,
		kAudioObjectPropertyElementMaster
	};

	return AudioObjectHasProperty(deviceid, &propertyAddress);
}

static Float32 SndCtlGetOutputDeviceFloatProperty(AudioObjectID deviceid, AudioObjectPropertySelector selector, CFErrorRef *error) {
	if (deviceid == kAudioDeviceUnknown)
		deviceid = SndCtlDefaultOutputDeviceID(NULL);
	if (deviceid == kAudioDeviceUnknown)
		return NAN;

	AudioObjectPropertyAddress propertyAddress = {
		selector,
		kAudioObjectPropertyScopeOutput,
		kAudioObjectPropertyElementMaster
	};

	Float32 value;
	UInt32 size = sizeof(value);
	OSStatus result = AudioObjectGetPropertyData(deviceid, &propertyAddress, 0, NULL, &size, &value);

	if (result != kAudioHardwareNoError) {
		if (error) {
			CFStringRef localizedDescription = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("Couldn't get float property for device ID %d"), deviceid);
			*error = SndCtlErrorCreateWithOSStatus(result, localizedDescription);
			CFRelease(localizedDescription);
		}
	}

	if (size != sizeof(value))
		return NAN;

	return value;
}

static bool SndCtlSetOutputDeviceFloatProperty(AudioObjectID deviceid, AudioObjectPropertySelector selector, Float32 value, CFErrorRef *error) {
	if (deviceid == kAudioDeviceUnknown)
		deviceid = SndCtlDefaultOutputDeviceID(error);

	if (deviceid == kAudioDeviceUnknown)
		return false;

	AudioObjectPropertyAddress propertyAddress = {
		selector,
		kAudioObjectPropertyScopeOutput,
		kAudioObjectPropertyElementMaster
	};

	OSStatus result = AudioObjectSetPropertyData(deviceid, &propertyAddress, 0, NULL, sizeof(value), &value);

	if (result != kAudioHardwareNoError) {
		if (error) {
			CFStringRef localizedDescription = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("Couldn't set float property for device ID %d"), deviceid);
			*error = SndCtlErrorCreateWithOSStatus(result, localizedDescription);
			CFRelease(localizedDescription);
		}
	}

	return result;
}


bool SndCtlOutputDeviceHasMainVolume(AudioDeviceID deviceid) {
	return SndCtlOutputDeviceHasProperty(deviceid, kAudioHardwareServiceDeviceProperty_VirtualMainVolume);
}

bool SndCtlOutputDeviceHasMainBalance(AudioDeviceID deviceid) {
	return SndCtlOutputDeviceHasProperty(deviceid, kAudioHardwareServiceDeviceProperty_VirtualMainBalance);
}

bool SndCtlSetVolume(AudioObjectID deviceid, Float32 volume, CFErrorRef *error) {
	return SndCtlSetOutputDeviceFloatProperty(deviceid, kAudioHardwareServiceDeviceProperty_VirtualMainVolume, volume, error);
}

bool SndCtlSetBalance(AudioObjectID deviceid, Float32 balance, CFErrorRef *error) {
	return SndCtlSetOutputDeviceFloatProperty(deviceid, kAudioHardwareServiceDeviceProperty_VirtualMainBalance, balance, error);
}

Float32 SndCtlGetVolume(AudioObjectID deviceid, CFErrorRef *error) {
	return SndCtlGetOutputDeviceFloatProperty(deviceid, kAudioHardwareServiceDeviceProperty_VirtualMainVolume, error);
}

Float32 SndCtlGetBalance(AudioObjectID deviceid, CFErrorRef *error) {
	return SndCtlGetOutputDeviceFloatProperty(deviceid, kAudioHardwareServiceDeviceProperty_VirtualMainBalance, error);
}

bool SndCtlIncrementBalance(AudioObjectID deviceid, Float32 delta, CFErrorRef *error) {
	Float32 balance = SndCtlGetBalance(deviceid, error);

	if (!isnan(balance))
		return SndCtlSetBalance(deviceid, balance + delta, error);

	return false;
}

bool SndCtlIncrementVolume(AudioObjectID deviceid, Float32 delta, CFErrorRef *error) {
	Float32 volume = SndCtlGetVolume(deviceid, error);

	if (!isnan(volume))
		return SndCtlSetVolume(deviceid, volume + delta, error);

	return false;
}

CFArrayRef SndCtlCopyAudioDevicesMatchingString(const char *stringToMatch, CFErrorRef *error) {
	CFArrayRef devices = SndCtlCopyAudioOutputDevices(error);

	if (!devices)
		return NULL;

	CFIndex count = CFArrayGetCount(devices);
	CFMutableArrayRef matchedDevices = CFArrayCreateMutable(kCFAllocatorDefault, count, &kCFTypeArrayCallBacks);

	CFStringRef cfStr = CFStringCreateWithCString(kCFAllocatorDefault, stringToMatch, kCFStringEncodingUTF8);

	for (CFIndex i = 0; i < count; ++i) {
		CFDictionaryRef device = CFArrayGetValueAtIndex(devices, i);
		CFStringRef name = CFDictionaryGetValue(device, CFSTR("name"));

		CFRange range = CFStringFind(name, cfStr, kCFCompareCaseInsensitive);

		if (range.location != kCFNotFound)
			CFArrayAppendValue(matchedDevices, device);
	}

	CFRelease(cfStr);
	CFRelease(devices);

	return matchedDevices;
}
