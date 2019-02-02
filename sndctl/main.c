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
	// Convert from Mac Roman to UTF-8
	char macstr[5];
	macstr[0] = (code & 0xff000000) >> 24;
	macstr[1] = (code & 0x00ff0000) >> 16;
	macstr[2] = (code & 0x0000ff00) >> 8;
	macstr[3] = code & 0x000000ff;
	macstr[4] = '\0';

	iconv_t converter = iconv_open("UTF-8", "MACINTOSH");
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

bool printVolume(AudioObjectID deviceid) {
	Float32 volume = SndCtlGetCurrentVolume(deviceid, NULL);

	if (!isnan(volume)) {
		printf("Volume: %.2f\n", volume);
		return true;
	}

	return false;
}

bool printBalance(AudioObjectID deviceid) {
	Float32 balance = SndCtlGetCurrentBalance(deviceid, NULL);

	if (!isnan(balance)) {
		if (balance == 0.0)
			printf("Balance: left\n");
		else if (balance == 0.5)
			printf("Balance: center\n");
		else if (balance == 1.0)
			printf("Balance: right\n");
		else
			printf("Balance: %.2f\n", balance);

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
		 "  -l, --list                 List available output devices.\n"
		 "  -h, --help                 Display this help.\n"
		 "  -V, --version              Display version information.\n"
		 );
}

static bool isDelta(char *str) {
	return str && strlen(str) > 1 && (str[0] == '+' || str[0] == '-');
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
					deviceid = SndCtlAudioDeviceStartingWithString(optarg, NULL);
					printf("Using device id %u.\n", deviceid);
				}

				break;
			case 'D': {
				shouldPrintUsage = false;
				AudioObjectID newDefaultId = (AudioObjectID)strtoul(optarg, NULL, 10);
				
				if (newDefaultId == 0 && errno == EINVAL) {
					newDefaultId = SndCtlAudioDeviceStartingWithString(optarg, NULL);
					printf("Setting default device id to %u.\n", newDefaultId);
				}

				SndCtlSetDefaultOutputDeviceID(newDefaultId, &error);
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
			printBalance(deviceid);
		if (shouldPrintVolume)
			printVolume(deviceid);
	}

	if (error) {
		SndCtlPrintError(error, true);
		return 1;
	}

	if (shouldPrintUsage)
		printUsage();

	return 0;
}
