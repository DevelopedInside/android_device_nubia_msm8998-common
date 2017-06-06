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

#define BREATH_SOURCE_NOTIFICATION	0x01
#define BREATH_SOURCE_BATTERY		0x02
#define BREATH_SOURCE_BUTTONS		0x04
#define BREATH_SOURCE_ATTENTION		0x08
#define BREATH_SOURCE_NONE			0x00

char const*const LCD_FILE
        = "/sys/class/leds/lcd-backlight/brightness";

/*
enum led_control_mode {	
    RGB_LED_MODE_CLOSED = 0,
    RGB_LED_MODE_CONSTANT_ON,
    RGB_LED_MODE_OFF,
    RGB_LED_MODE_AUTO_BLINK,
    RGB_LED_MODE_POWER_ON,
    RGB_LED_MODE_POWER_OFF,
    RGB_LED_MODE_ONCE_BLINK,	
};
*/
#define BLINK_MODE_ON			1
#define BLINK_MODE_OFF			2
#define BLINK_MODE_BREATH		3
#define BLINK_MODE_BREATH_ONCE	6
#define BLINK_MODE_UNKNOWN		0xff

/*
red_led: qcom,rgb_0		//home
	qcom,id = <3>;
	qcom,led_channel = <0x10>;
	qcom,is_auto_breath = <1>;
	nubia,fade_time = <3>;
	nubia,fullon_time = <0>;
	nubia,fulloff_time = <4>;
	nubia,onceblink_max_grade = <70>;
	nubia,autoblink_max_grade = <100>;
	nubia,grade_percentage = <100>;
	nubia,grade_scale_offset = <10> ;
blue_led: qcom,rgb_2	//button1?
	qcom,id = <4>;
	qcom,led_channel = <8>;
	qcom,is_auto_breath = <0>;
	nubia,grade_percentage = <30>;
	nubia,grade_scale_offset = <0> ;
blue_led: qcom,rgb_2	//button2?
	qcom,id = <5>;
	qcom,led_channel = <8>;
	qcom,is_auto_breath = <0>;
	nubia,grade_percentage = <30>;
	nubia,grade_scale_offset = <0> ;
all:
	qcom,pause-lo = <0>;
	qcom,pause-hi = <0>;
	qcom,pwm-us = <1000>;
	qcom,duty-pcts= [02 04 06 08 09 0a 0c 0e 10 11 12 14 16 18 19 1a 1b 1c 1e 20 22 24 26 28 
		19 2a 2c 2e 2f 30 32 34 36 38 39 3a 3b 3c 3d 3e 40 42 44 46 48 49 4a 4b 4c 4e 50 52 54 56 58 59 5a 5b 5c 5e 60 62 64];
*/
#define LED_CHANNEL_HOME	16
#define LED_CHANNEL_BUTTON	8

#define LED_GRADE_BUTTON			8
#define LED_GRADE_HOME				8
#define LED_GRADE_HOME_BATTERY_LOW	0
#define LED_GRADE_HOME_NOTIFICATION	6
#define LED_GRADE_HOME_BATTERY		6

char const*const LED_IN_MODE_BLINK
        = "/sys/class/leds/nubia_led/blink_mode";

char const*const LED_IN_SWITCH
        = "/sys/class/leds/nubia_led/outn";


/*
#define CONST_MIN_GRADE  1
#define CONST_MAX_GRADE  255

static int grade_parameter_convert(struct qpnp_led_data *led,int led_grade_temp)
{
	if(led_grade_temp){
		if((led_grade_temp*led->rgb_cfg->grade_percentage) < 100)		//if grade_temp * qcom,grade_percentage < 100 set 1
			led_grade_temp = CONST_MIN_GRADE;
		else															//else set grade_temp * qcom,grade_percentage / 100 + qcom,grade_scale_offset
			led_grade_temp = led_grade_temp*led->rgb_cfg->grade_percentage/100 + led->rgb_cfg->grade_scale_offset;
		if(led_grade_temp > CONST_MAX_GRADE)							//if grade_temp > 255 set 255
			led_grade_temp = CONST_MAX_GRADE;
	}

	 return led_grade_temp;
}

switch (led_param->ztemt_mode){
	case RGB_LED_MODE_CLOSED:
	case RGB_LED_MODE_OFF:
		[...]
		 break;
	case RGB_LED_MODE_CONSTANT_ON:				//grade_temp = min_grade ,  brightness = convert(temp_grade)
		//            *  when led constant on, keep five multiple of min grade            
		led_grade_temp=led_param->min_grade;
		pwm_cfg->blinking = true;
			pwm_cfg->mode = PWM_MODE;
		led->cdev.brightness = grade_parameter_convert(led,led_grade_temp);
		 break;
	case RGB_LED_MODE_ONCE_BLINK:				//not auto_breath (button): temp_grade = min_grade, brightness = convert(temp_grade); else (home): max_grade = qcom,onceblink_max_grade, blink once
		if (!led->rgb_cfg->is_auto_breath)
		{    
			led_grade_temp=led_param->min_grade;
			pwm_cfg->blinking = true;
			pwm_cfg->mode = PWM_MODE;
			led->cdev.brightness = grade_parameter_convert(led,led_grade_temp);
		}
		else
		{
			led_param->max_grade = led->rgb_cfg->onceblink_max_grade;
			qpnp_led_fill_parameter_breath_blink(led_param,pwm_cfg,loop);
			led->cdev.brightness=led->cdev.max_brightness;
		}
		break;            
	case RGB_LED_MODE_AUTO_BLINK:				//set fade, brightness = qcom,max_brightness, blink loop
		loop = true;
		led_param->fade_time= led->rgb_cfg->autoblink_fade_time;
		led_param->fullon_time= led->rgb_cfg->autoblink_fullon_time;
		led_param->fulloff_time = led->rgb_cfg->autoblink_fulloff_time;
		led_param->max_grade = led->rgb_cfg->autoblink_max_grade;
		qpnp_led_fill_parameter_breath_blink(led_param, pwm_cfg,loop);
		led->cdev.brightness=led->cdev.max_brightness;
		break;
		default:
		return -EINVAL;     
}

 *args=	mingrade maxgrade
 maxgrade always be filled by kernel with onceblink_max_grade/autoblink_max_grade when set to blink mode and connot be commited, so just set mingrade
 after set, set blink_mode to commit
*/
char const*const LED_IN_GRADE_PARA
        = "/sys/class/leds/nubia_led/grade_parameter";


/*
pwm_cfg->lut_params.ramp_step_ms = (fade_parameter_convert(led_param->fade_time) / pwm_cfg->duty_cycles->num_duty_pcts);
pwm_cfg->lut_params.lut_pause_hi = fade_parameter_convert(led_param->fullon_time);
pwm_cfg->lut_params.lut_pause_lo = fade_parameter_convert(led_param->fulloff_time);

#define FADE_PARAM_CONVERT 400

static int fade_parameter_convert(int temp_start)	// 2^(n-1) *400
{
	 int temp_end;
	 
	 if(temp_start<=0)
	     temp_end=0;
	 else
	     temp_end=(1<<(temp_start-1))*FADE_PARAM_CONVERT;

	 return temp_end;
}

- qcom,pause-lo: pause at low end of cycle
- qcom,pause-hi: pause at high end of cycle
- qcom,ramp-step-ms: step between each cycle (ms)

 *args=	fadetime ontime offtime
	eg: 3 0 4
 these settings will be filled by kernel when set to auto_blink and cannot be commited druing blink loop
*/
char const*const LED_IN_FADE_PARA
        = "/sys/class/leds/nubia_led/fade_parameter";


/*
char const*const BATTERY_CAPACITY
        = "/sys/class/power_supply/battery/capacity";

char const*const BATTERY_IS_CHARGING
        = "/sys/class/power_supply/battery/status";
*/


struct led_data
{
	int status;
	int min_grade;
	int fade_time;
	int fade_on_time;
	int fade_off_time;
};

static int active_status = 0;

static struct led_data current_home_led_status = {BLINK_MODE_UNKNOWN, -1, -1, -1, -1};		//status=BLINK_MODE_UNKNOWN, force write data on boot
static struct led_data current_button_led_status = {BLINK_MODE_UNKNOWN, -1, -1, -1, -1};		//status=BLINK_MODE_UNKNOWN, force write data on boot

#define true	1
#define false	0

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

inline int get_max(int a, int b)
{
	return a > b ? a : b;
}

/*
 * select a led
 * args:
 *	id: identify of led
 */
static void set_led_selected(int id)
{
	write_int(LED_IN_SWITCH, id);
}

/*
 * set min grade of a led
 * need to select a led using set_led_selected(id)
 * args:
 * 	min_grade: min grade of this led (brightness/min brightness in breath)
 */
static void set_led_current_grade(int min_grade)
{
	write_int(LED_IN_GRADE_PARA, min_grade);
}

/*
 * set fade of a led
 * need to select a led using select_led_selected(id)
 * Warning: these args only take effect in mode BREATH_ONCE
 * args:
 *	fade_time
 *	on_time
 *	off_time
 */
static void set_led_current_fade(int fade_time, int on_time, int off_time)
{
	char str[20];
	sprintf(str, "%d %d %d",fade_time, on_time, off_time);
	write_str(LED_IN_FADE_PARA, str);
}

/*
 * set mode of a led
 * need to select a led using select_led_selected(id)
 * args:
 *	mode: mode of this led
 */
static void set_led_current_mode(int mode)
{
	write_int(LED_IN_MODE_BLINK, mode);
}

/*
 * compare two led status
 * args:
 *	*led1, *led2: led_data
 * return:
 *	1: equal
 *	0: not equal
 */
static int compare_led_status(struct led_data *led1, struct led_data *led2)
{
	if (led1 == NULL || led2 == NULL) return false;
	if (led1->status != led2->status) return false;
	if (led1->min_grade != led2->min_grade) return false;
	if (led1->fade_time != led2->fade_time) return false;
	if (led1->fade_on_time != led2->fade_on_time) return false;
	if (led1->fade_off_time != led2->fade_off_time) return false;
	return true;
}

/*
 * copy led status
 * args:
 *	*to: copy to
 *	*from: copy from
 *	taken: is this status already be taken? affect status when status is BREATH_ONCE
 */
static void copy_led_status(struct led_data *to, struct led_data *from, int taken)
{
	if (to == NULL || from == NULL) return;
	if(taken && from->status == BLINK_MODE_BREATH_ONCE)		//after BREATH_ONCE, LED will always on
		to->status = BLINK_MODE_ON;
	else to->status = from->status;
	to->min_grade = from->min_grade;
	to->fade_time = from->fade_time;
	to->fade_on_time = from->fade_on_time;
	to->fade_off_time = from->fade_off_time;
}

/*
 * set home led status
 * args:
 *	*mode: led mode
 */
static void set_led_home_status(struct led_data *mode)
{
	if(mode == NULL) return;
	if( !compare_led_status(mode, &current_home_led_status) )
	{
		ALOGE("Write Home Status");
		set_led_selected(LED_CHANNEL_HOME);
		if(mode->min_grade >= 0)
			set_led_current_grade(mode->min_grade);
		if(mode->fade_time >= 0)
			set_led_current_fade(mode->fade_time, mode->fade_on_time, mode->fade_off_time);
		set_led_current_mode(mode->status);
		copy_led_status(&current_home_led_status, mode, true);
	}
}

/*
 * set button led status
 * args:
 *	*mode: led mode
 */
static void set_led_button_status(struct led_data *mode)
{
	if(mode == NULL) return;
	if( !compare_led_status(mode, &current_button_led_status) )
	{
		ALOGE("Write Button Status");
		set_led_selected(LED_CHANNEL_BUTTON);
		if(mode->min_grade >= 0)
			set_led_current_grade(mode->min_grade);
		//button led not support fade
		set_led_current_mode(mode->status);
		copy_led_status(&current_button_led_status, mode, true);
	}
}

/*
 * get led status string
 * args:
 *	*mode: led_data
 *	*string: string buffer
 */
static char* get_led_status_string(struct led_data * mode, char *string)
{
	switch(mode->status)
	{
		case BLINK_MODE_ON:
			sprintf(string,"On,Grade=%d",mode->min_grade);
			break;
		case BLINK_MODE_OFF:
			sprintf(string,"Off");
			break;
		case BLINK_MODE_BREATH:
			sprintf(string,"Breath,Grade=%d,Fade=%d,OnTime=%d,OffTime=%d",
				mode->min_grade, mode->fade_time, mode->fade_on_time, mode-> fade_off_time);
			break;
		default:
			sprintf(string,"Unknown");
			break;
	}
	return string;
}

static void log_led_status(struct led_data * home_mode, struct led_data * button_mode)
{
	char string1[48],string2[48];
	if(home_mode == NULL || button_mode == NULL) return;
	ALOGE("Home LED [%s] / Button LED [%s]\n", get_led_status_string(home_mode,string1),
		get_led_status_string(button_mode, string2) );
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
	struct led_data home_status = {BLINK_MODE_OFF, -1, -1, -1, -1};
	struct led_data button_status = {BLINK_MODE_OFF, -1, -1, -1, -1};

    if (brightness > 0) {
        active_status |= event_source;
    } else {
        active_status &= ~event_source;
	}

	if(active_status == 0) //nothing, close all
	{
		set_led_home_status(&home_status);
		set_led_button_status(&button_status);
		log_led_status(&home_status, &button_status);

		return 0;
	}
	if(active_status & BREATH_SOURCE_BUTTONS) //button backlight, turn all on
	{
		home_status.status = BLINK_MODE_ON;
		home_status.min_grade = LED_GRADE_HOME;
		
		button_status.status = BLINK_MODE_ON;
		button_status.min_grade = LED_GRADE_BUTTON;
	}

	if(active_status & BREATH_SOURCE_BATTERY) //battery status, set home on
	{
		home_status.status = BLINK_MODE_ON;
		home_status.min_grade = get_max(home_status.min_grade, LED_GRADE_HOME_BATTERY);
	}

	if( (active_status & BREATH_SOURCE_NOTIFICATION ) || (active_status & BREATH_SOURCE_ATTENTION) ) //notification, set home breath
	{
		home_status.status = BLINK_MODE_BREATH;
		home_status.min_grade = get_max(home_status.min_grade, LED_GRADE_HOME_NOTIFICATION);
	}

	set_led_home_status(&home_status);
	set_led_button_status(&button_status);
	log_led_status(&home_status, &button_status);
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
