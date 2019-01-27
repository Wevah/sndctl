//
//  SndCtlAudioUtils.c
//  sndctl
//
//  Created by Nate Weaver on 2019-01-27.
//  Copyright © 2019 Nate Weaver/Derailer. All rights reserved.
//

#include "SndCtlAudioUtils.h"

CFStringRef SndCtlCopyNameOfDeviceID(AudioObjectID devid) {
	AudioObjectPropertyAddress theAddress = {
		kAudioObjectPropertyName,
		kAudioObjectPropertyScopeOutput,
		0
	};

	CFStringRef name;
	UInt32 maxlen = sizeof(name);

	OSStatus result = AudioObjectGetPropertyData(devid, &theAddress, 0, NULL, &maxlen, &name);

	if (result != kAudioHardwareNoError)
		dprintf(STDERR_FILENO, "AudioObjectGetPropertyData: %d\n", result);

	return name;
}

UInt32 SndCtlNumberOfChannelsOfDeviceID(AudioObjectID devid) {
	AudioObjectPropertyAddress theAddress = {
		kAudioDevicePropertyStreamConfiguration,
		kAudioDevicePropertyScopeOutput,
		0
	};

	UInt32 propSize;
	UInt32 numberOfChannels = 0;

	OSStatus result = AudioObjectGetPropertyDataSize(devid, &theAddress, 0, NULL, &propSize);

	if (result != kAudioHardwareNoError)
		return 0;

	AudioBufferList *buflist = (AudioBufferList *)malloc(propSize);

	result = AudioObjectGetPropertyData(devid, &theAddress, 0, NULL, &propSize, buflist);

	if (result == kAudioHardwareNoError) {
		for (UInt32 i = 0; i < buflist->mNumberBuffers; ++i)
			numberOfChannels += buflist->mBuffers[i].mNumberChannels;
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

	if (result != kAudioHardwareNoError)
		dprintf(STDERR_FILENO, "AudioObjectGetPropertyDataSize: %d\n", result);

	int nDevices = propsize / sizeof(AudioObjectID);
	AudioObjectID devids[nDevices];
	result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &theAddress, 0, NULL, &propsize, devids);

	if (result != kAudioHardwareNoError)
		dprintf(STDERR_FILENO, "AudioObjectGetPropertyData: %d\n", result);

	CFMutableArrayRef devices = CFArrayCreateMutable(kCFAllocatorDefault, nDevices, &kCFTypeArrayCallBacks);

	for (int i = 0; i < nDevices; ++i) {
		if (SndCtlNumberOfChannelsOfDeviceID(devids[i]) > 0) {
			CFStringRef name = SndCtlCopyNameOfDeviceID(devids[i]);

			CFTypeRef keys[] = { CFSTR("id"), CFSTR("name") };
			CFNumberRef idNumber = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &devids[i]);
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

void SndCtlSetDefaultOutputDeviceID(AudioObjectID deviceID) {
	AudioObjectPropertyAddress defaultOutputDevicePropertyAddress = {
		kAudioHardwarePropertyDefaultOutputDevice,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster
	};

	UInt32 deviceIDSize = sizeof(deviceID);
	OSStatus result = AudioObjectSetPropertyData(kAudioObjectSystemObject, &defaultOutputDevicePropertyAddress, 0, NULL, deviceIDSize, &deviceID);

	if (result != kAudioHardwareNoError) {
		dprintf(STDERR_FILENO, "AudioObjectSetPropertyData: %d\n", result);
	}
}

char *SndCtlNameForDeviceProperty(AudioObjectPropertySelector selector) {
	if (selector == kAudioHardwareServiceDeviceProperty_VirtualMasterBalance)
		return "balance";
	else if (selector == kAudioHardwareServiceDeviceProperty_VirtualMasterVolume)
		return "volume";

	return NULL;
}

OSStatus SndCtlGetOutputDeviceFloatProperty(AudioObjectID devid, AudioObjectPropertySelector selector, Float32 *value) {
	if (devid == 0)
		devid = SndCtlDefaultOutputDeviceID();
	if (devid == 0)
		return false;

	AudioObjectPropertyAddress propertyAddress = {
		selector,
		kAudioObjectPropertyScopeOutput,
		kAudioObjectPropertyElementMaster
	};

	UInt32 size = sizeof(*value);
	OSStatus result = AudioObjectGetPropertyData(devid, &propertyAddress, 0, NULL, &size, value);

	if (size != sizeof(*value))
		return false;

//	if (result != kAudioHardwareNoError)
//		SndCtlPrintInfoForError(devid, selector, result, false);

	return result;
}

OSStatus SndCtlSetDeviceFloatProperty(AudioObjectID devid, AudioObjectPropertySelector selector, Float32 value) {
	if (devid == 0)
		devid = SndCtlDefaultOutputDeviceID();

	if (devid == 0)
		return false;

	AudioObjectPropertyAddress propertyAddress = {
		selector,
		kAudioObjectPropertyScopeOutput,
		kAudioObjectPropertyElementMaster
	};

	OSStatus result = AudioObjectSetPropertyData(devid, &propertyAddress, 0, NULL, sizeof(value), &value);

//	if (result != kAudioHardwareNoError)
//		SndCtlPrintInfoForError(devid, selector, result, true);

	return result;
}

OSStatus SndCtlSetVolume(AudioObjectID devid, Float32 volume) {
	return SndCtlSetDeviceFloatProperty(devid, kAudioHardwareServiceDeviceProperty_VirtualMasterVolume, volume);
}

OSStatus SndCtlSetBalance(AudioObjectID devid, Float32 balance) {
	return SndCtlSetDeviceFloatProperty(devid, kAudioHardwareServiceDeviceProperty_VirtualMasterBalance, balance);
}

OSStatus SndCtlGetCurrentVolume(AudioObjectID devid, Float32 *volume) {
	bool result = SndCtlGetOutputDeviceFloatProperty(devid, kAudioHardwareServiceDeviceProperty_VirtualMasterVolume, volume);
	return result;
}

OSStatus SndCtlGetCurrentBalance(AudioObjectID devid, Float32 *balance) {
	return SndCtlGetOutputDeviceFloatProperty(devid, kAudioHardwareServiceDeviceProperty_VirtualMasterBalance, balance);
}

OSStatus SndCtlIncrementBalance(AudioObjectID devid, Float32 delta) {
	Float32 balance;
	OSStatus result = SndCtlGetCurrentBalance(devid, &balance);

	if (result == kAudioHardwareNoError)
		return SndCtlSetBalance(devid, balance + delta);

	return result;
}

OSStatus SndCtlIncrementVolume(AudioObjectID devid, Float32 delta) {
	Float32 volume;
	OSStatus result = SndCtlGetCurrentVolume(devid, &volume);

	if (result == kAudioHardwareNoError)
		return SndCtlSetVolume(devid, volume + delta);

	return result;
}

AudioObjectID SndCtlAudioDeviceStartingWithString(char *prefix) {
	CFArrayRef devices = SndCtlCopyAudioOutputDevices();
	AudioObjectID devid = kAudioDeviceUnknown;

	CFIndex count = CFArrayGetCount(devices);
	CFStringRef cfPrefix = CFStringCreateWithCString(kCFAllocatorDefault, prefix, kCFStringEncodingUTF8);

	for (CFIndex i = 0; i < count; ++i) {
		CFDictionaryRef device = CFArrayGetValueAtIndex(devices, i);
		CFStringRef name = CFDictionaryGetValue(device, CFSTR("name"));

		CFRange range = CFStringFind(name, cfPrefix, kCFCompareAnchored | kCFCompareCaseInsensitive);

		if (range.location != kCFNotFound) {
			if (devid == kAudioDeviceUnknown) {
				CFNumberRef devidnum = CFDictionaryGetValue(device, CFSTR("id"));
				CFNumberGetValue(devidnum, kCFNumberIntType, &devid);
			} else {
				devid = kAudioDeviceUnknown;
				break;
			}
		}
	}

	CFRelease(cfPrefix);
	CFRelease(devices);

	return devid;
}

char *SndCtlInfoForError(AudioObjectID devid, AudioObjectPropertySelector selector, OSStatus result, bool isSetter) {
	static char infoString[256];

	switch (result) {
		case kAudioHardwareBadObjectError:
			snprintf(infoString, sizeof(infoString), "No audio device exists with ID %u!\n", devid);
			break;
		case kAudioHardwareUnknownPropertyError:
		{
			const char *selectorName = SndCtlNameForDeviceProperty(selector);
			char *action = isSetter ? "setting" : "getting";

			if (selectorName)
				snprintf(infoString, sizeof(infoString), "The audio device with ID %u doesn't support %s the %s!\n", devid, action, selectorName);
			else
				snprintf(infoString, sizeof(infoString), "The audio device with ID %u doesn't support %s the specified property!\n", devid, action);

			break;
		}
		default:
			snprintf(infoString, sizeof(infoString), "Audio Object Error: %d", result);
			break;
	}

	return infoString;
}

void SndCtlPrintInfoForError(AudioObjectID devid, AudioObjectPropertySelector selector, OSStatus result, bool isSetter) {
	dprintf(STDERR_FILENO, "%s", SndCtlInfoForError(devid, selector, result, isSetter));
}
