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

static CFErrorRef SndCtlErrorCreateWithOSStatus(OSStatus status, CFStringRef localizedFailure) {
	CFTypeRef keys[] = { kCFErrorLocalizedDescriptionKey };
	CFTypeRef values[] = { localizedFailure };
	CFDictionaryRef userInfo = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFErrorRef error =  CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainOSStatus, status, userInfo);
	CFRelease(userInfo);

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

CFArrayRef SndCtlCopyAudioOutputDevices(void) {
	UInt32 propsize;

	AudioObjectPropertyAddress theAddress = {
		kAudioHardwarePropertyDevices,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster
	};

	OSStatus result = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &theAddress, 0, NULL, &propsize);

	// FIXME: Return a CFErrorRef and don't print from this.
	if (result != kAudioHardwareNoError)
		dprintf(STDERR_FILENO, "AudioObjectGetPropertyDataSize: %d\n", result);

	int nDevices = propsize / sizeof(AudioObjectID);
	AudioObjectID deviceids[nDevices];
	result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &theAddress, 0, NULL, &propsize, deviceids);

	if (result != kAudioHardwareNoError)
		dprintf(STDERR_FILENO, "AudioObjectGetPropertyData: %d\n", result);

	CFMutableArrayRef devices = CFArrayCreateMutable(kCFAllocatorDefault, nDevices, &kCFTypeArrayCallBacks);

	for (int i = 0; i < nDevices; ++i) {
		if (SndCtlNumberOfChannelsOfDeviceID(deviceids[i], NULL) > 0) {
			CFStringRef name = SndCtlCopyNameOfDeviceID(deviceids[i], NULL);

			CFTypeRef keys[] = { kSndCtlAudioDeviceAttributeID, kSndCtlAudioDeviceAttributeName };
			CFNumberRef idNumber = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &deviceids[i]);
			CFTypeRef values[] = { idNumber, name };

			CFDictionaryRef device = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			CFArrayAppendValue(devices, device);

			CFRelease(idNumber);
			CFRelease(name);
		}
	}

	return devices;
}

AudioObjectID SndCtlDefaultOutputDeviceID(void) {
	AudioObjectPropertyAddress defaultOutputDevicePropertyAddress = {
		kAudioHardwarePropertyDefaultOutputDevice,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster
	};

	AudioObjectID defaultOutputDeviceID;
	UInt32 deviceIDSize = sizeof(defaultOutputDeviceID);
	OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &defaultOutputDevicePropertyAddress, 0, NULL, &deviceIDSize, &defaultOutputDeviceID);

	if (result != kAudioHardwareNoError) {
		dprintf(STDERR_FILENO, "AudioObjectGetPropertyData: %d\n", result);
		return 0;
	}

	return defaultOutputDeviceID;
}

OSStatus SndCtlSetDefaultOutputDeviceID(AudioObjectID deviceid) {
	AudioObjectPropertyAddress defaultOutputDevicePropertyAddress = {
		kAudioHardwarePropertyDefaultOutputDevice,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster
	};

	UInt32 deviceIDSize = sizeof(deviceid);
	OSStatus result = AudioObjectSetPropertyData(kAudioObjectSystemObject, &defaultOutputDevicePropertyAddress, 0, NULL, deviceIDSize, &deviceid);

	return result;
}

char *SndCtlNameForDeviceProperty(AudioObjectPropertySelector selector) {
	if (selector == kAudioHardwareServiceDeviceProperty_VirtualMasterBalance)
		return "balance";
	else if (selector == kAudioHardwareServiceDeviceProperty_VirtualMasterVolume)
		return "volume";

	return NULL;
}

static OSStatus SndCtlGetOutputDeviceFloatProperty(AudioObjectID deviceid, AudioObjectPropertySelector selector, Float32 *value) {
	if (deviceid == 0)
		deviceid = SndCtlDefaultOutputDeviceID();
	if (deviceid == 0)
		return false;

	AudioObjectPropertyAddress propertyAddress = {
		selector,
		kAudioObjectPropertyScopeOutput,
		kAudioObjectPropertyElementMaster
	};

	UInt32 size = sizeof(*value);
	OSStatus result = AudioObjectGetPropertyData(deviceid, &propertyAddress, 0, NULL, &size, value);

	if (size != sizeof(*value))
		return false;

	return result;
}

static OSStatus SndCtlSetOutputDeviceFloatProperty(AudioObjectID deviceid, AudioObjectPropertySelector selector, Float32 value) {
	if (deviceid == 0)
		deviceid = SndCtlDefaultOutputDeviceID();

	if (deviceid == 0)
		return false;

	AudioObjectPropertyAddress propertyAddress = {
		selector,
		kAudioObjectPropertyScopeOutput,
		kAudioObjectPropertyElementMaster
	};

	OSStatus result = AudioObjectSetPropertyData(deviceid, &propertyAddress, 0, NULL, sizeof(value), &value);

	return result;
}

OSStatus SndCtlSetVolume(AudioObjectID deviceid, Float32 volume) {
	return SndCtlSetOutputDeviceFloatProperty(deviceid, kAudioHardwareServiceDeviceProperty_VirtualMasterVolume, volume);
}

OSStatus SndCtlSetBalance(AudioObjectID deviceid, Float32 balance) {
	return SndCtlSetOutputDeviceFloatProperty(deviceid, kAudioHardwareServiceDeviceProperty_VirtualMasterBalance, balance);
}

OSStatus SndCtlGetCurrentVolume(AudioObjectID deviceid, Float32 *volume) {
	bool result = SndCtlGetOutputDeviceFloatProperty(deviceid, kAudioHardwareServiceDeviceProperty_VirtualMasterVolume, volume);
	return result;
}

OSStatus SndCtlGetCurrentBalance(AudioObjectID deviceid, Float32 *balance) {
	return SndCtlGetOutputDeviceFloatProperty(deviceid, kAudioHardwareServiceDeviceProperty_VirtualMasterBalance, balance);
}

OSStatus SndCtlIncrementBalance(AudioObjectID deviceid, Float32 delta) {
	Float32 balance = 0.0;
	OSStatus result = SndCtlGetCurrentBalance(deviceid, &balance);

	if (result == kAudioHardwareNoError)
		return SndCtlSetBalance(deviceid, balance + delta);

	return result;
}

OSStatus SndCtlIncrementVolume(AudioObjectID deviceid, Float32 delta) {
	Float32 volume = 0.0;
	OSStatus result = SndCtlGetCurrentVolume(deviceid, &volume);

	if (result == kAudioHardwareNoError)
		return SndCtlSetVolume(deviceid, volume + delta);

	return result;
}

AudioObjectID SndCtlAudioDeviceStartingWithString(char *prefix) {
	CFArrayRef devices = SndCtlCopyAudioOutputDevices();
	AudioObjectID deviceid = kAudioDeviceUnknown;

	CFIndex count = CFArrayGetCount(devices);
	CFStringRef cfPrefix = CFStringCreateWithCString(kCFAllocatorDefault, prefix, kCFStringEncodingUTF8);

	for (CFIndex i = 0; i < count; ++i) {
		CFDictionaryRef device = CFArrayGetValueAtIndex(devices, i);
		CFStringRef name = CFDictionaryGetValue(device, CFSTR("name"));

		CFRange range = CFStringFind(name, cfPrefix, kCFCompareAnchored | kCFCompareCaseInsensitive);

		if (range.location != kCFNotFound) {
			if (deviceid == kAudioDeviceUnknown) {
				CFNumberRef deviceidnum = CFDictionaryGetValue(device, CFSTR("id"));
				CFNumberGetValue(deviceidnum, kCFNumberIntType, &deviceid);
			} else {
				deviceid = kAudioDeviceUnknown;
				break;
			}
		}
	}

	CFRelease(cfPrefix);
	CFRelease(devices);

	return deviceid;
}

char *SndCtlInfoForError(AudioObjectID deviceid, AudioObjectPropertySelector selector, OSStatus result, bool isSetter) {
	static char infoString[256];

	switch (result) {
		case kAudioHardwareBadObjectError:
			snprintf(infoString, sizeof(infoString), "No audio device exists with ID %u!", deviceid);
			break;
		case kAudioHardwareUnknownPropertyError:
		{
			const char *selectorName = SndCtlNameForDeviceProperty(selector);
			char *action = isSetter ? "setting" : "getting";

			if (selectorName)
				snprintf(infoString, sizeof(infoString), "The audio device with ID %u doesn't support %s the %s!", deviceid, action, selectorName);
			else
				snprintf(infoString, sizeof(infoString), "The audio device with ID %u doesn't support %s the specified property!", deviceid, action);

			break;
		}
		default:
			snprintf(infoString, sizeof(infoString), "Audio Object Error: %d", result);
			break;
	}

	return infoString;
}

void SndCtlPrintInfoForError(AudioObjectID deviceid, AudioObjectPropertySelector selector, OSStatus result, bool isSetter) {
	dprintf(STDERR_FILENO, "%s\n", SndCtlInfoForError(deviceid, selector, result, isSetter));
}
