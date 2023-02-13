//
//  main.c
//  sndctl
//
//  Created by Nate Weaver on 2017-01-13.
//  Copyright © 2017 Nate Weaver/Derailer. All rights reserved.
//

// Using #import instead of #include since it's pretty certain that the compiler will
// support it, given that sndctl is Mac-only.

#import <xlocale.h>
#import <getopt.h>
#import <iconv.h>
#import "SndCtlAudioUtils.h"

char *utf8StringCopyFromCFString(CFStringRef string, char *buf, size_t buflen) {
	const char *cStr = CFStringGetCStringPtr(string, kCFStringEncodingUTF8);

	if (!cStr)
		CFStringGetCString(string, buf, buflen, kCFStringEncodingUTF8);
	else
		strlcpy(buf, cStr, buflen);

	return buf;
}

char *SndControlStringFromFourCharCode(FourCharCode code) {
	// Most of the Audio Toolbox codes only use ASCII chars,
	// but just to be safe, convert from Mac Roman to UTF-8
	// for display.
	char macstr[5];
	macstr[0] = (code & 0xff000000) >> 24;
	macstr[1] = (code & 0x00ff0000) >> 16;
	macstr[2] = (code & 0x0000ff00) >> 8;
	macstr[3] = code & 0x000000ff;
	macstr[4] = '\0';

	iconv_t converter = iconv_open("UTF-8", "MACINTOSH");

	// outstr is static so we can just return and the caller doesn't have to free it.
	static char outstr[16];
	char *inbuf = (char *)macstr;
	char *outbuf = (char *)outstr;
	size_t insize = sizeof(macstr);
	size_t outsize = sizeof(outstr);
	size_t result = iconv(converter, &inbuf, &insize, &outbuf, &outsize);

	if (result < 0)
		dprintf(STDERR_FILENO, "iconv error: %d\n", errno);

	iconv_close(converter);

	return outstr;
}

void SndCtlPrintError(CFErrorRef error, bool release) {
	char buf[256];
	CFStringRef localizedDescription = CFErrorCopyDescription(error);
	CFIndex code = CFErrorGetCode(error);
	utf8StringCopyFromCFString(localizedDescription, buf, sizeof(buf));

	dprintf(STDERR_FILENO, "%s (%s)\n", buf, SndControlStringFromFourCharCode((FourCharCode)code));
	CFRelease(localizedDescription);

	if (release)
		CFRelease(error);
}

void listAudioOutputDevices(void) {
	CFErrorRef error;
	CFArrayRef devices = SndCtlCopyAudioOutputDevices(&error);

	if (!devices) {
		SndCtlPrintError(error, true);
		return;
	}

	CFIndex count = CFArrayGetCount(devices);

	for (CFIndex i = 0; i < count; ++i) {
		CFDictionaryRef device = CFArrayGetValueAtIndex(devices, i);
		CFNumberRef idval = CFDictionaryGetValue(device, CFSTR("id"));
		AudioObjectID id;
		CFNumberGetValue(idval, kCFNumberSInt32Type, &id);
		CFStringRef name = CFDictionaryGetValue(device, CFSTR("name"));

		char nameBuffer[256];

		utf8StringCopyFromCFString(name, nameBuffer, sizeof(nameBuffer));

		printf("%d: %s\n", id, nameBuffer);
	}

	CFRelease(devices);
}

// Counts the codepoints in a UTF-8 string.
size_t count_codepoints(const char *str) {
	size_t count = 0;
	size_t length = strlen(str);

	// Count bytes that aren't continuation byets; i.e., those that don't match with 0b11xxxxxx.
	for (int i = 0; i < length; ++i) {
		if ((str[i] & 0xc0) != 0x80)
			++count;
	}

	return count;
}

void SndCtlPrintSlider(size_t barWidth, Float32 position, const char *minString, const char *maxString) {
	if (barWidth < 5)
		return;

	if (barWidth > 200) {
		dprintf(STDERR_FILENO, "barWidth too long.\n");
		return;
	}

	static const char * const knobString = "#";
	static const char * const barFill = "=";
	static const char * const barLeftCap = "[";
	static const char * const barRightCap = "]";

	static const char * const bold = "\033[1m";
	static const char * const normal = "\033[0m";

	if (!minString)
		minString = "";
	if (!maxString)
		maxString = "";

	size_t fillWidth = barWidth - 2;

	char barString[256];
	size_t barFillLength = strlen(barFill);

	size_t knobLocation = round(position * (fillWidth - 1));
	size_t bytelocation = 0;
	size_t characterlocation = 0;

	while (characterlocation < fillWidth) {
		if (characterlocation == knobLocation) {
			memcpy(barString + bytelocation, knobString, strlen(knobString));
			bytelocation += strlen(knobString);
			characterlocation += 1;
		} else {
			memcpy(barString + bytelocation, barFill, barFillLength);
			bytelocation += barFillLength;
			characterlocation += barFillLength;
		}
	}

	barString[bytelocation] = '\0';

	printf("%s%s%s" "%s%s%s" "%s%s%s\n", bold, minString, normal, barLeftCap, barString, barRightCap, bold, maxString, normal);
}

bool printVolume(AudioObjectID deviceid, bool printAsSlider, CFErrorRef *error) {
	Float32 volume = SndCtlGetVolume(deviceid, error);

	if (!isnan(volume)) {
		if (printAsSlider)
			SndCtlPrintSlider(21, volume, "- ", " +");
		else
			printf("Volume: %.2f\n", volume);
		return true;
	}

	return false;
}

bool printBalance(AudioObjectID deviceid, bool printAsSlider, CFErrorRef *error) {
	Float32 balance = SndCtlGetBalance(deviceid, error);

	if (!isnan(balance)) {
		if (printAsSlider) {
			SndCtlPrintSlider(21, balance, "L ", " R");
		} else {
			if (balance == 0.0)
				printf("Balance: left\n");
			else if (balance == 0.5)
				printf("Balance: center\n");
			else if (balance == 1.0)
				printf("Balance: right\n");
			else
				printf("Balance: %.2f\n", balance);
		}

		return true;
	}

	return false;
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
		 "      --visual               Display -V and -B as ASCII sliders.\n"
		 "  -l, --list                 List available output devices.\n"
		 "  -h, --help                 Display this help.\n"
		 "  -V, --version              Display version information.\n"
		 );
}

static inline bool isDelta(char *str) {
	return str && strlen(str) > 1 && (str[0] == '+' || str[0] == '-');
}

bool SndCtlHandleDeviceMatchingAndPrintErrors(const char *stringToMatch, AudioDeviceID *deviceid) {
	CFErrorRef error;
	CFArrayRef matchedDevices = SndCtlCopyAudioDevicesMatchingString(stringToMatch, &error);

	if (!matchedDevices) {
		SndCtlPrintError(error, true);
		return false;
	}

	*deviceid = kAudioDeviceUnknown;
	CFIndex count = CFArrayGetCount(matchedDevices);

	switch (count) {
		case 0:
			dprintf(STDERR_FILENO, "'%s' didn't match any devices.\n", stringToMatch);
			CFRelease(matchedDevices);
			return false;
			break;
		case 1: {
			CFDictionaryRef device = CFArrayGetValueAtIndex(matchedDevices, 0);
			CFNumberRef deviceIdRef = CFDictionaryGetValue(device, kSndCtlAudioDeviceAttributeID);
			CFNumberGetValue(deviceIdRef, kCFNumberSInt32Type, deviceid);
			CFRelease(matchedDevices);
			return true;
			break;
		}
		default:
			dprintf(STDERR_FILENO, "'%s' matched more than one device:\n", stringToMatch);

			for (CFIndex i = 0; i < count; ++i) {
				AudioDeviceID iteratedDeviceID;
				CFDictionaryRef device = CFArrayGetValueAtIndex(matchedDevices, i);
				CFNumberRef deviceIdRef = CFDictionaryGetValue(device, kSndCtlAudioDeviceAttributeID);
				CFNumberGetValue(deviceIdRef, kCFNumberSInt32Type, &iteratedDeviceID);

				CFStringRef nameRef = CFDictionaryGetValue(device, kSndCtlAudioDeviceAttributeName);
				char nameStr[256];
				utf8StringCopyFromCFString(nameRef, nameStr, sizeof(nameStr));

				dprintf(STDERR_FILENO, "  %s (%u)\n", nameStr, iteratedDeviceID);
			}
			break;
	}

	CFRelease(matchedDevices);

	return false;
}

int main(int argc, const char * argv[]) {
	static struct option longopts[] = {
		{ "balance",		required_argument,	NULL,	'b' },
		{ "printbalance",	no_argument,		NULL,	'B' },
		{ "volume",			required_argument,	NULL,	'v' },
		{ "printvolume",	no_argument,		NULL,	'V' },
		{ "default",		required_argument,	NULL,	'D' },
		{ "device",			required_argument,	NULL,	'd' },

		{ "visual", 		no_argument,		NULL,	'visu' },

		{ "help",			no_argument,		NULL,	'h' },
		{ "list",			no_argument,		NULL,	'l' },
		{ "version",		no_argument,		NULL,	'vers' },
		{ NULL,				0,					NULL,	0 }
	};

	int opt;
	AudioObjectID deviceid = 0;

	Float32 balance = 0.5;
	bool shouldSetBalance = false;

	Float32 volume = 0.0;
	bool shouldSetVolume = false;

	bool shouldPrintUsage = true;

	bool shouldPrintVolume = false;
	bool shouldPrintBalance = false;
	bool balanceIsDelta = false;
	bool volumeIsDelta = false;

	CFErrorRef error = NULL;

	bool printAsSlider = false;

	while ((opt = getopt_long(argc, (char * const *)argv, "b:Bv:Vd:D:hl", longopts, NULL)) != -1) {
		switch (opt) {
			case 'b': {
				shouldSetBalance = true;
				shouldPrintUsage = false;

				balanceIsDelta = isDelta(optarg);

				char *endptr;
				balance = strtof_l(optarg, &endptr, NULL); // Always use the C locale.

				// Balance synonyms.
				if (balance == 0.0 && endptr && endptr == optarg) {
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

				volumeIsDelta = isDelta(optarg);

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
				deviceid = (AudioObjectID)strtoul(optarg, NULL, 10);

				if (deviceid == 0 && errno == EINVAL) {
					if (SndCtlHandleDeviceMatchingAndPrintErrors(optarg, &deviceid))
						printf("Using device id %u.\n", deviceid);
					else
						return 1;
				}

				break;
			case 'D': {
				shouldPrintUsage = false;
				AudioObjectID newDefaultId = (AudioObjectID)strtoul(optarg, NULL, 10);
				
				if (newDefaultId == 0 && errno == EINVAL) {
					if (!SndCtlHandleDeviceMatchingAndPrintErrors(optarg, &newDefaultId))
						return 1;
				}

				printf("Setting default device id to %u.\n", newDefaultId);
				SndCtlSetDefaultOutputDeviceID(newDefaultId, &error);
				break;
			}
			case 'vers':
				printVersion();
				return 0;
				break;
			case 'visu':
				printAsSlider = true;
				break;
		}
	}

//	argc -= optind;
//	argv += optind;

	if (!error) {
		if (shouldSetBalance) {
			if (balanceIsDelta)
				SndCtlIncrementBalance(deviceid, balance, &error);
			else
				SndCtlSetBalance(deviceid, balance, &error);
		}

		if (shouldSetVolume) {
			if (volumeIsDelta)
				SndCtlIncrementVolume(deviceid, volume, &error);
			else
				SndCtlSetVolume(deviceid, volume, &error);
		}

		if (shouldPrintBalance)
			printBalance(deviceid, printAsSlider, &error);
		if (shouldPrintVolume)
			printVolume(deviceid, printAsSlider, &error);
	}

	if (error) {
		SndCtlPrintError(error, true);
		return 1;
	}

	if (shouldPrintUsage)
		printUsage();

	return 0;
}
