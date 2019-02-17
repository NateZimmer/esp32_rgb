/*
 * led.h
 *
 *  Created on: Feb 16, 2019
 *      Author: nate
 */

#ifndef MAIN_LED_H_
#define MAIN_LED_H_

#include <sys/param.h>

#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "driver/ledc.h"

typedef struct rgb{
	uint32_t red_duty;
	uint32_t green_duty;
	uint32_t blue_duty;
}rgb_duty;

extern rgb_duty rgb_led;


void blink_task(void *pvParameter);
void pwm_task(void *pvParameter);
void parse_rgb(const char * str);


#endif /* MAIN_LED_H_ */
