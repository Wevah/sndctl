//
//  main.c
//  centerbalance
//
//  Created by Nate Weaver on 2017-01-13.
//  Copyright © 2017 Nate Weaver/Derailer. All rights reserved.
//

#import <AudioToolbox/AudioToolbox.h>
#import <xlocale.h>

int main(int argc, const char * argv[]) {
	Float32 balance = 0.5;

	if (argc > 1) {
		char *endptr;
		balance = strtof_l(argv[1], &endptr, NULL); // Always use the C locale.

		if (balance == 0.0 && endptr == argv[1])
			balance = 0.5;
		else {
			balance = balance < 0.0 ? 0.0 : balance;
			balance = balance > 1.0 ? 1.0 : balance;
		}
	}

	AudioObjectPropertyAddress getDefaultOutputDevicePropertyAddress = {
		kAudioHardwarePropertyDefaultOutputDevice,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMaster
	};

	AudioDeviceID defaultOutputDeviceID;
	UInt32 volumedataSize = sizeof(defaultOutputDeviceID);
	OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &getDefaultOutputDevicePropertyAddress, 0, NULL, &volumedataSize, &defaultOutputDeviceID);

	if (result != kAudioHardwareNoError) {
		dprintf(STDERR_FILENO, "AudioObjectGetPropertyData: %d", result);
		return 1;
	}

	AudioObjectPropertyAddress balancePropertyAddress = {
		kAudioHardwareServiceDeviceProperty_VirtualMasterBalance,
		kAudioObjectPropertyScopeOutput,
		kAudioObjectPropertyElementMaster
	};

	volumedataSize = sizeof(balance);
	result = AudioObjectSetPropertyData(defaultOutputDeviceID, &balancePropertyAddress, 0, NULL, sizeof(balance), &balance);

	if (result != kAudioHardwareNoError) {
		dprintf(STDERR_FILENO, "AudioObjectSetPropertyData: %d", result);
		return 1;
	}

	return 0;
}
