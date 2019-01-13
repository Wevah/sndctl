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

CFStringRef copyNameOfDeviceID(AudioObjectID devid) CF_RETURNS_RETAINED {
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

void listAudioOutputDevices(void) {
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

	for (int i = 0; i < nDevices; ++i) {
		if (numberOfChannelsOfDeviceID(devids[i]) > 0) {
			CFStringRef name = copyNameOfDeviceID(devids[i]);
			const char *cName = CFStringGetCStringPtr(name, kCFStringEncodingUTF8);
			char nameBuffer[64];

			if (!cName) {
				CFStringGetCString(name, nameBuffer, sizeof(nameBuffer), kCFStringEncodingUTF8);
				cName = nameBuffer;
			}

			printf("%u: %s\n", devids[i], cName);

			CFRelease(name);
		}
	}
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

bool setDeviceProperty(AudioObjectID devid, AudioObjectPropertySelector selector, Float32 value) {
	if (devid == 0)
		devid = defaultOutputDeviceID();

	if (devid == 0)
		return false;

	AudioObjectPropertyAddress volumePropertyAddress = {
		selector,
		kAudioObjectPropertyScopeOutput,
		kAudioObjectPropertyElementMaster
	};

	OSStatus result = AudioObjectSetPropertyData(devid, &volumePropertyAddress, 0, NULL, sizeof(value), &value);

	switch (result) {
		case kAudioHardwareNoError:
			break;
		case kAudioHardwareBadObjectError:
			dprintf(STDERR_FILENO, "No audio device exists with ID %u!\n", devid);
			break;
		case kAudioHardwareUnknownPropertyError:
		{
			const char *selectorName = NULL;

			if (selector == kAudioHardwareServiceDeviceProperty_VirtualMasterBalance)
				selectorName = "balance";
			else if (selector == kAudioHardwareServiceDeviceProperty_VirtualMasterVolume)
				selectorName = "volume";

			if (selectorName)
				dprintf(STDERR_FILENO, "The audio device with ID %u doesn't support setting the %s!\n", devid, selectorName);
			else
				dprintf(STDERR_FILENO, "The audio device with ID %u doesn't support setting the specified property!\n", devid);

			break;
		}
		default:
			dprintf(STDERR_FILENO, "AudioObjectSetPropertyData: %d", result);
			break;
	}

	return result == kAudioHardwareNoError;
}

bool setVolume(AudioObjectID devid, Float32 volume) {
	return setDeviceProperty(devid, kAudioHardwareServiceDeviceProperty_VirtualMasterVolume, volume);
}

bool setBalance(AudioObjectID devid, Float32 balance) {
	return setDeviceProperty(devid, kAudioHardwareServiceDeviceProperty_VirtualMasterBalance, balance);
}

char *utf8StringCopyFromCFString(CFStringRef string, char *buf, size_t buflen) {
	const char *cStr = CFStringGetCStringPtr(string, kCFStringEncodingUTF8);

	if (!cStr)
		CFStringGetCString(string, buf, buflen, kCFStringEncodingUTF8);
	else
		strlcpy(buf, cStr, buflen);

	return buf;
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
		 "  -v, --volume=<volume>      Set the volume from 0.0 (mute) to 1.0 (max).\n"
		 "  -d, --device=<deviceid>    Use the specified device ID instead of the current output device.\n"
		 "  -D, --default=<deviceid>   Set the default audio device.\n"
		 "  -l, --list                 List available output devices.\n"
		 "  -h, --help                 Display this help.\n"
		 "  -V, --version              Display version information.\n"
		 );
}

int main(int argc, const char * argv[]) {
	static struct option longopts[] = {
		{ "balance",	required_argument,	NULL,	'b' },
		{ "volume",		required_argument,	NULL,	'v' },
		{ "default",	required_argument,	NULL,	'D' },
		{ "device",		required_argument,	NULL,	'd' },

		{ "help",		no_argument,		NULL,	'h' },
		{ "list",		no_argument,		NULL,	'l' },
		{ "version",	no_argument,		NULL,	'V' },
		{ NULL,			0,					NULL,	0 }
	};

	int opt;
	AudioObjectID devid = 0;

	Float32 balance = 0.5;
	bool shouldSetBalance = false;

	Float32 volume = 0.0;
	bool shouldSetVolume = false;

	bool shouldPrintUsage = true;

	while ((opt = getopt_long(argc, (char * const *)argv, "b:v:d:D:hlV", longopts, NULL)) != -1) {
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
			case 'v': {
				shouldSetVolume = true;
				shouldPrintUsage = false;

				char *endptr;
				volume = strtof_l(optarg, &endptr, NULL); // Always use the C locale.

				break;
			}
			case 'D': {
				shouldPrintUsage = false;
				AudioObjectID devid = (AudioObjectID)strtoul(optarg, NULL, 10);
				setDefaultOutputDeviceID(devid);
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
				break;
			case 'V':
				printVersion();
				return 0;
				break;
		}
	}

	argc -= optind;
	argv += optind;

	if (shouldSetBalance)
		setBalance(devid, balance);

	if (shouldSetVolume)
		setVolume(devid, volume);

	if (shouldPrintUsage)
		printUsage();

	return 0;
}
