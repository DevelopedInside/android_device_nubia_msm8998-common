/*
 * Copyright (C) 2008 The Android Open Source Project
 * Copyright (C) 2014 The Linux Foundation. All rights reserved.
 * Copyright (C) 2016 The CyanogenMod Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cutils/log.h>
#include <cutils/properties.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <math.h>

#include <sys/ioctl.h>
#include <sys/types.h>

#include <hardware/lights.h>

#define LOG_TAG "lightHAL"

/******************************************************************************/

static pthread_once_t g_init = PTHREAD_ONCE_INIT;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

static struct light_state_t g_attention;
static struct light_state_t g_notification;
static struct light_state_t g_battery;
static struct light_state_t g_buttons;

#define BREATH_SOURCE_NOTIFICATION  0x01
#define BREATH_SOURCE_BATTERY       0x02
#define BREATH_SOURCE_BUTTONS       0x04
#define BREATH_SOURCE_ATTENTION     0x08
#define BREATH_SOURCE_NONE          0x00

//static int last_state = BREATH_SOURCE_NONE;


char const*const LCD_FILE
        = "/sys/class/leds/lcd-backlight/brightness";


char const*const LED_IN_MODE_BLINK
        = "/sys/class/leds/nubia_led/blink_mode";

char const*const LED_IN_SWITCH
        = "/sys/class/leds/nubia_led/outn";

char const*const LED_IN_GRADE_PARA
        = "/sys/class/leds/nubia_led/grade_parameter";

/*
char const*const BATTERY_CAPACITY
        = "/sys/class/power_supply/battery/capacity";

char const*const BATTERY_IS_CHARGING
        = "/sys/class/power_supply/battery/status";
*/
#define BLINK_MODE_ON		6
#define BLINK_MODE_BREATH	3
#define BLINK_MODE_OFF		2

#define LED_CHANNEL_HOME	16
#define LED_CHANNEL_BUTTON	8

#define LED_GRADE_BUTTON	3
#define LED_GRADE_HOME		8

static int active_status = BREATH_SOURCE_NONE;

static int cur_home_led_status = BLINK_MODE_OFF;
static int cur_button_led_status = BLINK_MODE_OFF;

/**
 * Device methods
 */

static void init_globals(void)
{
    // Init the mutex
    pthread_mutex_init(&g_lock, NULL);
}

static int write_int(char const* path, int value)
{
    int fd;
    static int already_warned = 0;

    fd = open(path, O_RDWR);
    if (fd >= 0) {
        char buffer[20];
        int bytes = sprintf(buffer, "%d\n", value);
        int amt = write(fd, buffer, bytes);
        close(fd);
        return amt == -1 ? -errno : 0;
    } else {
        if (already_warned == 0) {
            ALOGE("write_int failed to open %s\n", path);
            already_warned = 1;
        }
        return -errno;
    }
}

static int write_str(char const* path, char* value)
{
    int fd;
    static int already_warned = 0;

    fd = open(path, O_RDWR);
    if (fd >= 0) {
        char buffer[PAGE_SIZE];
        int bytes = sprintf(buffer, "%s\n", value);
        int amt = write(fd, buffer, bytes);
        close(fd);
        return amt == -1 ? -errno : 0;
    } else {
        if (already_warned == 0) {
            ALOGE("write_str failed to open %s\n", path);
            already_warned = 1;
        }
        return -errno;
    }
}

static int rgb_to_brightness(struct light_state_t const* state)
{
    int color = state->color & 0x00ffffff;
    return (
      ((color >> 16) & 0xff)
    + ((color >> 8 ) & 0xff)
    + ( color        & 0xff)
    ) / 3;
}

static void set_led_home_status(int mode)
{
	if(mode != cur_home_led_status)
	{
		write_int(LED_IN_SWITCH, LED_CHANNEL_HOME);
		write_int(LED_IN_GRADE_PARA, LED_GRADE_HOME);
		write_int(LED_IN_MODE_BLINK, mode);
		cur_home_led_status = mode;
	}
}

static void set_led_button_status(int mode)
{
	if(mode != cur_button_led_status)
	{
		write_int(LED_IN_SWITCH, LED_CHANNEL_BUTTON);
		write_int(LED_IN_GRADE_PARA, LED_GRADE_BUTTON);
		write_int(LED_IN_MODE_BLINK, mode);
		cur_button_led_status = mode;
	}
}

static char* get_led_status_string(int mode)
{
	char* string;
	switch(mode)
	{
		case BLINK_MODE_ON:
			string = "On";
			break;
		case BLINK_MODE_OFF:
			string = "Off";
			break;
		case BLINK_MODE_BREATH:
			string = "Breath";
			break;
		default:
			string = "Unknown";
			break;
	}
	return string;
}

static void log_led_status(int home_mode, int button_mode)
{
	ALOGE("Home LED %s / Button LED %s\n", get_led_status_string(home_mode), get_led_status_string(button_mode) );
}

static int set_light_backlight(struct light_device_t* dev,
        struct light_state_t const* state)
{
    int err = 0;
    int brightness = rgb_to_brightness(state);
    pthread_mutex_lock(&g_lock);
    err = write_int(LCD_FILE, brightness);
    pthread_mutex_unlock(&g_lock);
    return err;
}

static int set_breath_light_locked(int event_source,
        struct light_state_t const* state)
{
    char* blink_mode;

    int brightness = rgb_to_brightness(state);
	int home_status = BLINK_MODE_OFF;
	int button_status = BLINK_MODE_OFF;

    if (brightness > 0) {
        active_status |= event_source;
    } else {
        active_status &= ~event_source;
	}

	if(active_status == 0) //nothing, close all
	{
		set_led_home_status(BLINK_MODE_OFF);
		set_led_button_status(BLINK_MODE_OFF);
		log_led_status(BLINK_MODE_OFF, BLINK_MODE_OFF);

		return 0;
	}
	if(active_status & BREATH_SOURCE_BUTTONS) //button backlight, turn all on
	{
		home_status = BLINK_MODE_ON;
		button_status = BLINK_MODE_ON;
	}

	if(active_status & BREATH_SOURCE_BATTERY) //battery status, set home on
	{
		home_status = BLINK_MODE_ON;
	}

	if( (active_status & BREATH_SOURCE_NOTIFICATION ) || (active_status & BREATH_SOURCE_ATTENTION) ) //notification, set home breath
	{
		home_status = BLINK_MODE_BREATH;
	}

	set_led_home_status(home_status);
	set_led_button_status(button_status);
	log_led_status(home_status, button_status);
    return 0;
}

static int set_light_buttons(struct light_device_t* dev,
        struct light_state_t const* state)
{
    pthread_mutex_lock(&g_lock);
    g_buttons = *state;
    set_breath_light_locked(BREATH_SOURCE_BUTTONS, &g_buttons);
    pthread_mutex_unlock(&g_lock);
    return 0;
}

static int set_light_battery(struct light_device_t* dev,
        struct light_state_t const* state)
{
    pthread_mutex_lock(&g_lock);
    g_battery = *state;
    set_breath_light_locked(BREATH_SOURCE_BATTERY, &g_battery);
    pthread_mutex_unlock(&g_lock);
    return 0;
}

static int set_light_notifications(struct light_device_t* dev,
        struct light_state_t const* state)
{
    pthread_mutex_lock(&g_lock);
    g_notification = *state;
    set_breath_light_locked(BREATH_SOURCE_NOTIFICATION, &g_notification);
    pthread_mutex_unlock(&g_lock);
    return 0;
}

static int set_light_attention(struct light_device_t* dev,
        struct light_state_t const* state)
{
    pthread_mutex_lock(&g_lock);
    g_attention = *state;
    set_breath_light_locked(BREATH_SOURCE_ATTENTION, &g_attention);
    pthread_mutex_unlock(&g_lock);
    return 0;
}

/** Close the lights device */
static int close_lights(struct light_device_t *dev)
{
    if (dev) {
        free(dev);
    }
    return 0;
}


/**
 * Module methods
 */

/** Open a new instance of a lights device using name */
static int open_lights(const struct hw_module_t* module, char const* name,
        struct hw_device_t** device)
{
    int (*set_light)(struct light_device_t* dev,
            struct light_state_t const* state);

    if (0 == strcmp(LIGHT_ID_BACKLIGHT, name))
        set_light = set_light_backlight;
    else if (0 == strcmp(LIGHT_ID_BUTTONS, name))
        set_light = set_light_buttons;
    else if (0 == strcmp(LIGHT_ID_BATTERY, name))
        set_light = set_light_battery;
    else if (0 == strcmp(LIGHT_ID_NOTIFICATIONS, name))
        set_light = set_light_notifications;
    else if (0 == strcmp(LIGHT_ID_ATTENTION, name))
        set_light = set_light_attention;
    else
        return -EINVAL;

    pthread_once(&g_init, init_globals);

    struct light_device_t *dev = malloc(sizeof(struct light_device_t));
    memset(dev, 0, sizeof(*dev));

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = (struct hw_module_t*)module;
    dev->common.close = (int (*)(struct hw_device_t*))close_lights;
    dev->set_light = set_light;

    *device = (struct hw_device_t*)dev;
    return 0;
}

static struct hw_module_methods_t lights_module_methods = {
    .open =  open_lights,
};

/*
 * The lights Module
 */
struct hw_module_t HAL_MODULE_INFO_SYM = {
    .tag = HARDWARE_MODULE_TAG,
    .version_major = 1,
    .version_minor = 0,
    .id = LIGHTS_HARDWARE_MODULE_ID,
    .name = "Lights Module for Nubia Z11",
    .author = "Parheliamm, XiNGRZ, RichardTung",
    .methods = &lights_module_methods,
};
