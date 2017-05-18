//
//  main.c
//  centerbalance
//
//  Created by Nate Weaver on 2017-01-13.
//  Copyright © 2017 Nate Weaver/Derailer. All rights reserved.
//

#import <CoreAudio/CoreAudio.h>
#import <AudioToolbox/AudioToolbox.h>

int main(int argc, const char * argv[]) {
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
	}

	AudioObjectPropertyAddress balancePropertyAddress = {
		kAudioHardwareServiceDeviceProperty_VirtualMasterBalance,
		kAudioDevicePropertyScopeOutput,
		kAudioObjectPropertyElementMaster
	};

	Float32 balance = 0.5;
	volumedataSize = sizeof(balance);

	result = AudioObjectSetPropertyData(defaultOutputDeviceID,
										&balancePropertyAddress,
										0, NULL,
										sizeof(balance), &balance);
	if (result != kAudioHardwareNoError) {
		dprintf(STDERR_FILENO, "AudioObjectSetPropertyData: %d", result);
	}

	return 0;
}
