#include "gamepad.h"

#include <kinc/input/gamepad.h>

#include <fcntl.h>
#include <libudev.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>

#define ABS_INFO 24

struct HIDGamepad {
	int idx;
	char gamepad_dev_name[256];
	char name[385];
	int file_descriptor;
	bool connected;
	struct input_event gamepadEvent;
	struct input_absinfo abs[ABS_INFO];
};

static int skip[32]; static int skip_ct = 0;

static int eventToIndex(int e){
	int ct = 0; for(int i=0; i<skip_ct; i++) if(skip[i] == e) return -1; else if(skip[i] < e) ct++; return e-ct;
}

static bool HIDGamepad_open(struct HIDGamepad *pad, int e) {
	pad->file_descriptor = open(pad->gamepad_dev_name, O_RDONLY | O_NONBLOCK);
	if (pad->file_descriptor < 0) {
		pad->connected = false; if(errno == EACCES){if(skip_ct < 32) skip[skip_ct++] = e; return false;}
	}
	else {
		pad->connected = true;

		char buf[128];
		if (ioctl(pad->file_descriptor, EVIOCGNAME(sizeof(buf)), buf) < 0) {
			strncpy(buf, "Unknown", sizeof(buf));
		}
		for(int i=0; i<ABS_INFO; i++) ioctl(pad->file_descriptor, EVIOCGABS(i), &pad->abs[i]);
		snprintf(pad->name, sizeof(pad->name), "%s(%s)", buf, pad->gamepad_dev_name);
	} return true;
}

static void HIDGamepad_close(struct HIDGamepad *pad) {
	if (pad->connected) {
		close(pad->file_descriptor);
		pad->file_descriptor = -1;
		pad->connected = false;
	}
}

static int KEYS[16] = {BTN_A, BTN_B, BTN_X, BTN_Y, BTN_TL, BTN_TR, BTN_TL2, BTN_TR2, BTN_SELECT, BTN_START, BTN_THUMBL, BTN_THUMBR, BTN_DPAD_UP, BTN_DPAD_DOWN, BTN_DPAD_LEFT, BTN_DPAD_RIGHT};

void HIDGamepad_processEvent(struct HIDGamepad *pad, struct input_event e) {
	switch (e.type) {
	case EV_KEY:
		for(int i=0; i<16; i++) if(KEYS[i] == e.code){kinc_internal_gamepad_trigger_button(pad->idx, i, e.value); break;}
		break;
	case EV_ABS: if(e.code < ABS_INFO){
		float val = ((float)e.value) / pad->abs[e.code].maximum;
		switch(e.code){
			case ABS_Z: kinc_internal_gamepad_trigger_button(pad->idx, 6, val); break;
			case ABS_RZ: kinc_internal_gamepad_trigger_button(pad->idx, 7, val); break;
			case ABS_HAT0X: case ABS_HAT1X: case ABS_HAT2X: case ABS_HAT3X: kinc_internal_gamepad_trigger_button(pad->idx, 14, val < 0?-val:0); kinc_internal_gamepad_trigger_button(pad->idx, 15, val < 0?0:val); break;
			case ABS_HAT0Y: case ABS_HAT1Y: case ABS_HAT2Y: case ABS_HAT3Y: kinc_internal_gamepad_trigger_button(pad->idx, 12, val < 0?-val:0); kinc_internal_gamepad_trigger_button(pad->idx, 13, val < 0?0:val); break;
			default: kinc_internal_gamepad_trigger_axis(pad->idx, e.code, val); break;
		} break;
	}
	default:
		break;
	}
}

void HIDGamepad_update(struct HIDGamepad *pad) {
	if (pad->connected) {
		while (read(pad->file_descriptor, &pad->gamepadEvent, sizeof(pad->gamepadEvent)) > 0) {
			HIDGamepad_processEvent(pad, pad->gamepadEvent);
		}
	}
}

struct HIDGamepadUdevHelper {
	struct udev *udevPtr;
	struct udev_monitor *udevMonitorPtr;
	int udevMonitorFD;
};

static struct HIDGamepadUdevHelper udev_helper;

#define gamepadCount 12
static struct HIDGamepad gamepads[gamepadCount];

static void HIDGamepadUdevHelper_openOrCloseGamepad(struct HIDGamepadUdevHelper *helper, struct udev_device *dev) {
	const char *action = udev_device_get_action(dev);
	if (!action)
		action = "add";

	const char *joystickDevnodeName = strstr(udev_device_get_devnode(dev), "event");

	if (joystickDevnodeName) {
		int joystickDevnodeIndex;
		sscanf(joystickDevnodeName, "event%d", &joystickDevnodeIndex);
		int i = eventToIndex(joystickDevnodeIndex);
		if(i >= 0 && i < gamepadCount){
			if (!strcmp(action, "add")) {
				snprintf(gamepads[i].gamepad_dev_name, sizeof(gamepads[i].gamepad_dev_name), "/dev/input/event%d", joystickDevnodeIndex);
				HIDGamepad_open(&gamepads[i], joystickDevnodeIndex);
			}

			if (!strcmp(action, "remove")) {
				HIDGamepad_close(&gamepads[i]);
			}
		}
	}
}

static void HIDGamepadUdevHelper_processDevice(struct HIDGamepadUdevHelper *helper, struct udev_device *dev) {
	if (dev) {
		if (udev_device_get_devnode(dev))
			HIDGamepadUdevHelper_openOrCloseGamepad(helper, dev);

		udev_device_unref(dev);
	}
}

static void HIDGamepadUdevHelper_init(struct HIDGamepadUdevHelper *helper) {
	struct udev *udevPtrNew = udev_new();

	// enumerate
	struct udev_enumerate *enumerate = udev_enumerate_new(udevPtrNew);

	udev_enumerate_add_match_subsystem(enumerate, "input");
	udev_enumerate_scan_devices(enumerate);

	struct udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate);
	struct udev_list_entry *entry;

	udev_list_entry_foreach(entry, devices) {
		const char *path = udev_list_entry_get_name(entry);
		struct udev_device *dev = udev_device_new_from_syspath(udevPtrNew, path);
		HIDGamepadUdevHelper_processDevice(helper, dev);
	}

	udev_enumerate_unref(enumerate);

	// setup mon
	helper->udevMonitorPtr = udev_monitor_new_from_netlink(udevPtrNew, "udev");

	udev_monitor_filter_add_match_subsystem_devtype(helper->udevMonitorPtr, "input", NULL);
	udev_monitor_enable_receiving(helper->udevMonitorPtr);

	helper->udevMonitorFD = udev_monitor_get_fd(helper->udevMonitorPtr);

	helper->udevPtr = udevPtrNew;
}

static void HIDGamepadUdevHelper_update(struct HIDGamepadUdevHelper *helper) {
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(helper->udevMonitorFD, &fds);

	if (FD_ISSET(helper->udevMonitorFD, &fds)) {
		struct udev_device *dev = udev_monitor_receive_device(helper->udevMonitorPtr);
		HIDGamepadUdevHelper_processDevice(helper, dev);
	}
}

static void HIDGamepadUdevHelper_close(struct HIDGamepadUdevHelper *helper) {
	udev_unref(helper->udevPtr);
}

void kinc_linux_initHIDGamepads() {
	int i=0; int index = 0; while(i < gamepadCount && index < 40){
		struct HIDGamepad *pad = &gamepads[i];
		pad->file_descriptor = -1;
		pad->connected = false;
		pad->gamepad_dev_name[0] = 0;
		pad->idx = i;
		snprintf(pad->gamepad_dev_name, sizeof(pad->gamepad_dev_name), "/dev/input/event%d", index);
		if(HIDGamepad_open(pad, index)) i++; index++;
	}
	HIDGamepadUdevHelper_init(&udev_helper);
}

void kinc_linux_updateHIDGamepads() {
	HIDGamepadUdevHelper_update(&udev_helper);
	for (int i = 0; i < gamepadCount; ++i) {
		HIDGamepad_update(&gamepads[i]);
	}
}

void kinc_linux_closeHIDGamepads() {
	HIDGamepadUdevHelper_close(&udev_helper);
}

const char *kinc_gamepad_vendor(int gamepad) {
	return "Linux gamepad";
}

const char *kinc_gamepad_product_name(int gamepad) {
	return gamepads[gamepad].name;
}

bool kinc_gamepad_connected(int gamepad) {
	return gamepads[gamepad].connected;
}

void kinc_gamepad_rumble(int gamepad, float left, float right) {}
