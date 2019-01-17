//
//  main.c
//  sndctl
//
//  Created by Nate Weaver on 2017-01-13.
//  Copyright © 2017 Nate Weaver/Derailer. All rights reserved.
//

// Using #import instead of #include since it's pretty certain that the compiler will
// support it, given that sndctl is Mac-only.

#import <AudioToolbox/AudioToolbox.h>
#import <xlocale.h>
#import <getopt.h>

CFStringRef copyNameOfDeviceID(AudioObjectID devid) {
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

UInt32 numberOfChannelsOfDeviceID(AudioObjectID devid) {
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

CFArrayRef copyAudioOutputDevices(void) {
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
		if (numberOfChannelsOfDeviceID(devids[i]) > 0) {
			CFStringRef name = copyNameOfDeviceID(devids[i]);

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

char *utf8StringCopyFromCFString(CFStringRef string, char *buf, size_t buflen) {
	const char *cStr = CFStringGetCStringPtr(string, kCFStringEncodingUTF8);

	if (!cStr)
		CFStringGetCString(string, buf, buflen, kCFStringEncodingUTF8);
	else
		strlcpy(buf, cStr, buflen);

	return buf;
}

void listAudioOutputDevices(void) {
	CFArrayRef devices = copyAudioOutputDevices();
	CFIndex count = CFArrayGetCount(devices);

	for (CFIndex i = 0; i < count; ++i) {
		CFDictionaryRef device = CFArrayGetValueAtIndex(devices, i);
		CFNumberRef idval = CFDictionaryGetValue(device, CFSTR("id"));
		AudioObjectID id;
		CFNumberGetValue(idval, kCFNumberSInt32Type, &id);
		CFStringRef name = CFDictionaryGetValue(device, CFSTR("name"));

		char nameBuffer[64];

		utf8StringCopyFromCFString(name, nameBuffer, sizeof(nameBuffer));

		printf("%d: %s\n", id, nameBuffer);
	}

	CFRelease(devices);
}

AudioObjectID defaultOutputDeviceID(void) {
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

void setDefaultOutputDeviceID(AudioObjectID deviceID) {
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

char *nameForDeviceProperty(AudioObjectPropertySelector selector) {
	if (selector == kAudioHardwareServiceDeviceProperty_VirtualMasterBalance)
		return "balance";
	else if (selector == kAudioHardwareServiceDeviceProperty_VirtualMasterVolume)
		return "volume";

	return NULL;
}

void printInfoForError(AudioObjectID devid, AudioObjectPropertySelector selector, OSStatus result, bool isSetter) {
	switch (result) {
		case kAudioHardwareBadObjectError:
			dprintf(STDERR_FILENO, "No audio device exists with ID %u!\n", devid);
			break;
		case kAudioHardwareUnknownPropertyError:
		{
			const char *selectorName = nameForDeviceProperty(selector);
			char *action = isSetter ? "setting" : "getting";

			if (selectorName)
				dprintf(STDERR_FILENO, "The audio device with ID %u doesn't support %s the %s!\n", devid, action, selectorName);
			else
				dprintf(STDERR_FILENO, "The audio device with ID %u doesn't support %s the specified property!\n", devid, action);

			break;
		}
		default:
			dprintf(STDERR_FILENO, "AudioObjectSetPropertyData: %d", result);
			break;
	}
}

bool getOutputDeviceFloatProperty(AudioObjectID devid, AudioObjectPropertySelector selector, Float32 *value) {
	if (devid == 0)
		devid = defaultOutputDeviceID();
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

	if (result != kAudioHardwareNoError)
		printInfoForError(devid, selector, result, false);

	return result == kAudioHardwareNoError;
}

bool setDeviceProperty(AudioObjectID devid, AudioObjectPropertySelector selector, Float32 value) {
	if (devid == 0)
		devid = defaultOutputDeviceID();

	if (devid == 0)
		return false;

	AudioObjectPropertyAddress propertyAddress = {
		selector,
		kAudioObjectPropertyScopeOutput,
		kAudioObjectPropertyElementMaster
	};

	OSStatus result = AudioObjectSetPropertyData(devid, &propertyAddress, 0, NULL, sizeof(value), &value);

	if (result != kAudioHardwareNoError)
		printInfoForError(devid, selector, result, true);

	return result == kAudioHardwareNoError;
}

bool setVolume(AudioObjectID devid, Float32 volume) {
	return setDeviceProperty(devid, kAudioHardwareServiceDeviceProperty_VirtualMasterVolume, volume);
}

bool setBalance(AudioObjectID devid, Float32 balance) {
	return setDeviceProperty(devid, kAudioHardwareServiceDeviceProperty_VirtualMasterBalance, balance);
}

bool printVolume(AudioObjectID devid) {
	Float32 volume;
	bool result = getOutputDeviceFloatProperty(devid, kAudioHardwareServiceDeviceProperty_VirtualMasterVolume, &volume);

	if (result)
		printf("Volume: %.2f\n", volume);

	return result;
}

bool printBalance(AudioObjectID devid) {
	Float32 balance;
	bool result = getOutputDeviceFloatProperty(devid, kAudioHardwareServiceDeviceProperty_VirtualMasterBalance, &balance);

	if (result) {
		if (balance == 0.0)
			printf("Balance: left\n");
		else if (balance == 0.5)
			printf("Balance: center\n");
		else if (balance == 1.0)
			printf("Balance: right\n");
		else
			printf("Balance: %.2f\n", balance);
	}

	return result;
}

/**
 Returns the ID of the audio device whose prefix matches \c prefix\n.

 @param prefix The prefix to match, case-insensitively.
 @return The matched audio device ID, or kAudioDeviceUnknown the prefix does not match exactly one device name.
 */
AudioObjectID audioDeviceStartingWithString(char *prefix) {
	CFArrayRef devices = copyAudioOutputDevices();
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

void printVersion(void) {
	CFBundleRef bundle = CFBundleGetMainBundle();
	char shortVersion[64];
	char bundleVersion[64];
	char copyright[64];

	printf("\n"
		   "    sndctl version %s (v%s) " __DATE__ "\n"
		   "\n"
		   "    %s"
		   "\n\n",
		   utf8StringCopyFromCFString(CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("CFBundleShortVersionString")), shortVersion, sizeof(shortVersion)),
		   utf8StringCopyFromCFString(CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("CFBundleVersion")), bundleVersion, sizeof(bundleVersion)),
		   utf8StringCopyFromCFString(CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("NSHumanReadableCopyright")), copyright, sizeof(copyright))
		   );
}

void printUsage(void) {
	printf("\nUsage: %s [-b balance] [-v volume] [-d deviceid] [-l]\n\n", getprogname());
}

void printHelp(void) {
	printUsage();
	puts("Options:\n"
		 "  -b, --balance=<balance>    Set the balance from 0.0 (left) to 1.0 (right).\n"
		 "                             'l', 'r', and 'c' are synonyms for 0.0, 1.0, and 0.5, respectively.\n"
		 "  -B, --printbalance         Display the current balance.\n"
		 "  -v, --volume=<volume>      Set the volume from 0.0 (mute) to 1.0 (max).\n"
		 "  -V, --printvolume          Display the current volume.\n"
		 "  -d, --device=<device>      Modify the specified device instead of the default output device.\n"
		 "  -D, --default=<device>     Set the default audio device.\n"
		 "  -l, --list                 List available output devices.\n"
		 "  -h, --help                 Display this help.\n"
		 "  -V, --version              Display version information.\n"
		 );
}

int main(int argc, const char * argv[]) {
	static struct option longopts[] = {
		{ "balance",		required_argument,	NULL,	'b' },
		{ "printbalance",	no_argument,		NULL,	'B' },
		{ "volume",			required_argument,	NULL,	'v' },
		{ "printvolume",	no_argument,		NULL,	'V' },
		{ "default",		required_argument,	NULL,	'D' },
		{ "device",			required_argument,	NULL,	'd' },

		{ "help",			no_argument,		NULL,	'h' },
		{ "list",			no_argument,		NULL,	'l' },
		{ "version",		no_argument,		NULL,	'vers' },
		{ NULL,				0,					NULL,	0 }
	};

	int opt;
	AudioObjectID devid = 0;

	Float32 balance = 0.5;
	bool shouldSetBalance = false;

	Float32 volume = 0.0;
	bool shouldSetVolume = false;

	bool shouldPrintUsage = true;

	bool shouldPrintVolume = false;
	bool shouldPrintBalance = false;

	while ((opt = getopt_long(argc, (char * const *)argv, "b:Bv:Vd:D:hl", longopts, NULL)) != -1) {
		switch (opt) {
			case 'b': {
				shouldSetBalance = true;
				shouldPrintUsage = false;

				char *endptr;
				balance = strtof_l(optarg, &endptr, NULL); // Always use the C locale.

				// Balance synonyms.
				if (balance == 0.0 && endptr == optarg) {
					if (strlen(endptr) != 0) {
						switch(endptr[0]) {
							case 'l':
							case 'L':
								balance = 0.0;
								break;
							case 'r':
							case 'R':
								balance = 1.0;
								break;
							case 'c':
							case 'C':
								balance = 0.5;
								break;
							default:
								dprintf(STDERR_FILENO, "Invalid argument '%s' to option 'balance'.\n", endptr);
								shouldSetBalance = false;
								break;
						}
					} else
						balance = 0.5;
				}

				break;
			}
			case 'B': {
				shouldPrintBalance = true;
				shouldPrintUsage = false;
				break;
			}
			case 'v': {
				shouldSetVolume = true;
				shouldPrintUsage = false;

				char *endptr;
				volume = strtof_l(optarg, &endptr, NULL); // Always use the C locale.

				break;
			}
			case 'V': {
				shouldPrintVolume = true;
				shouldPrintUsage = false;
				break;
			}
			case 'h':
				printHelp();
				return 0;
				break;
			case 'l':
				listAudioOutputDevices();
				return 0;
				break;
			case 'd':
				devid = (AudioObjectID)strtoul(optarg, NULL, 10);

				if (devid == 0 && errno == EINVAL) {
					devid = audioDeviceStartingWithString(optarg);
					printf("Using device id %u.\n", devid);
				}

				break;
			case 'D': {
				shouldPrintUsage = false;
				AudioObjectID newDefaultId = (AudioObjectID)strtoul(optarg, NULL, 10);
				
				if (newDefaultId == 0 && errno == EINVAL) {
					newDefaultId = audioDeviceStartingWithString(optarg);
					printf("Setting default device id to %u.\n", newDefaultId);
				}

				setDefaultOutputDeviceID(newDefaultId);
				break;
			}
			case 'vers':
				printVersion();
				return 0;
				break;
		}
	}

//	argc -= optind;
//	argv += optind;

	if (shouldSetBalance)
		setBalance(devid, balance);

	if (shouldSetVolume)
		setVolume(devid, volume);

	if (shouldPrintBalance)
		printBalance(devid);
	if (shouldPrintVolume)
		printVolume(devid);

	if (shouldPrintUsage)
		printUsage();

	return 0;
}
