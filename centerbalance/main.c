//
//  main.c
//  centerbalance
//
//  Created by Nate Weaver on 2017-01-13.
//  Copyright © 2017 Nate Weaver/Derailer. All rights reserved.
//

#import <AudioToolbox/AudioToolbox.h>
#import <xlocale.h>
#import <getopt.h>

CFStringRef copyNameOfDeviceID(AudioDeviceID devid) CF_RETURNS_RETAINED {
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

UInt32 numberOfChannelsOfDeviceID(AudioDeviceID devid) {
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
	return result;
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

	int nDevices = propsize / sizeof(AudioDeviceID);
	AudioDeviceID devids[nDevices];
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
	AudioObjectPropertyAddress getDefaultOutputDevicePropertyAddress = {
		kAudioHardwarePropertyDefaultOutputDevice,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster
	};

	AudioDeviceID defaultOutputDeviceID;
	UInt32 deviceIDSize = sizeof(defaultOutputDeviceID);
	OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &getDefaultOutputDevicePropertyAddress, 0, NULL, &deviceIDSize, &defaultOutputDeviceID);

	if (result != kAudioHardwareNoError) {
		dprintf(STDERR_FILENO, "AudioObjectGetPropertyData: %d", result);
		return 0;
	}

	return defaultOutputDeviceID;
}

bool setBalance(AudioDeviceID devid, Float32 balance) {
	if (devid == 0)
		devid = defaultOutputDeviceID();

	if (devid == 0)
		return false;

	AudioObjectPropertyAddress balancePropertyAddress = {
		kAudioHardwareServiceDeviceProperty_VirtualMasterBalance,
		kAudioObjectPropertyScopeOutput,
		kAudioObjectPropertyElementMaster
	};

	OSStatus result = AudioObjectSetPropertyData(devid, &balancePropertyAddress, 0, NULL, sizeof(balance), &balance);

	switch (result) {
		case kAudioHardwareNoError:
			break;
		case kAudioHardwareBadObjectError:
			dprintf(STDERR_FILENO, "No audio device exists with ID %u!\n", devid);
			break;
		case kAudioHardwareUnknownPropertyError:
			dprintf(STDERR_FILENO, "The audio device with ID %u doesn't support setting the balance!\n", devid);
			break;
		default:
			dprintf(STDERR_FILENO, "AudioObjectSetPropertyData: %d", result);
			break;
	}

	return result == kAudioHardwareNoError;
}

void printHelp(void) {
	printf("\nUsage: %s [options] [balance]\n"
		   "Where balance is 0.0 (left) to 1.0 (right) and defaults to 0.5 (center)\n"
		   "\"l\" and \"r\" can also be used as synonyms for 0.0 and 1.0, respectively\n\n",
		   getprogname());
	puts("Options:\n" \
		 "  -d [deviceid]    Use the specified device ID instead of the current output device.\n"
		 "  -l               List available output devices.\n"
		 "  -h               Display this help.\n"
		 );
}

int main(int argc, const char * argv[]) {
	static struct option longopts[] = {
		{ "help",	no_argument,		NULL,	'h' },
		{ "list",	no_argument,		NULL,	'l' },
		{ "device", required_argument,	NULL,	'd'},
		{ NULL,		0,					NULL,	0	}
	};

	int opt;
	AudioObjectID devid = 0;

	while ((opt = getopt_long(argc, (char * const *)argv, "hld:", longopts, NULL)) != -1) {
		switch (opt) {
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
		}

		argc -= optind;
		argv += optind;
	}

	Float32 balance = 0.5;

	if (argc > 1) {
		char *endptr;
		balance = strtof_l(argv[1], &endptr, NULL); // Always use the C locale.

		if (balance == 0.0 && endptr == argv[1]) {
			if (strlen(endptr) != 0) {
				switch(endptr[0]) {
					case 'l':
						balance = 0.0;
						break;
					case 'r':
						balance = 1.0;
						break;
					default:
						balance = 0.5;
						break;
				}
			} else
				balance = 0.5;
		} else {
			balance = balance < 0.0 ? 0.0 : balance;
			balance = balance > 1.0 ? 1.0 : balance;
		}
	}

	return setBalance(devid, balance) ? 0 : 1;
}
