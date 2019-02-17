#include "led.h"

#define RGB_RED			GPIO_NUM_0
#define RGB_GREEN       GPIO_NUM_2
#define RGB_BLUE		GPIO_NUM_4

rgb_duty rgb_led = {
		.red_duty = 0,
		.green_duty = 0,
		.blue_duty = 0
};

ledc_channel_config_t green_channel = {
            .channel    = LEDC_CHANNEL_0,
            .duty       = 1<<(LEDC_TIMER_13_BIT-1),
            .gpio_num   = RGB_GREEN,
            .speed_mode = LEDC_HIGH_SPEED_MODE,
            .hpoint     = 0,
            .timer_sel  = LEDC_TIMER_0
};

ledc_channel_config_t red_channel = {
            .channel    = LEDC_CHANNEL_1,
            .duty       = 1<<(LEDC_TIMER_13_BIT-1),
            .gpio_num   = RGB_RED,
            .speed_mode = LEDC_HIGH_SPEED_MODE,
            .hpoint     = 0,
            .timer_sel  = LEDC_TIMER_0
};

ledc_channel_config_t blue_channel = {
            .channel    = LEDC_CHANNEL_2,
            .duty       = 1<<(LEDC_TIMER_13_BIT-1),
            .gpio_num   = RGB_BLUE,
            .speed_mode = LEDC_HIGH_SPEED_MODE,
            .hpoint     = 0,
            .timer_sel  = LEDC_TIMER_0
};


void parse_rgb(const char * str){
    char * ptr = str;
    char * ptr2;
    int val = 0;

    val = strtoul (ptr, &ptr2, 0);
    ptr = ptr2; ptr++;

    rgb_led.red_duty = (uint32_t)val;

    val = strtoul (ptr, &ptr2, 0);
    ptr = ptr2; ptr++;

    rgb_led.green_duty = (uint32_t)val;

    val = strtoul (ptr, &ptr2, 0);
    ptr = ptr2; ptr++;

    rgb_led.blue_duty = (uint32_t)val;
}


void update_rgb_duty(rgb_duty * rgb){
	red_channel.duty = ((0x1<<LEDC_TIMER_13_BIT)-1)*rgb->red_duty/1000;
	green_channel.duty = ((0x1<<LEDC_TIMER_13_BIT)-1)*rgb->green_duty/1000;
	blue_channel.duty = ((0x1<<LEDC_TIMER_13_BIT)-1)*rgb->blue_duty/1000;
	ledc_channel_config(&red_channel);
	ledc_channel_config(&green_channel);
	ledc_channel_config(&blue_channel);
}


void pwm_task(void *pvParameter)
{
	ledc_timer_config_t ledc_timer = {
	        .duty_resolution = LEDC_TIMER_13_BIT, // resolution of PWM duty
	        .freq_hz = 5000,                      // frequency of PWM signal
	        .speed_mode = LEDC_HIGH_SPEED_MODE,           // timer mode
	        .timer_num = LEDC_TIMER_0            // timer index
	};
	ledc_timer_config(&ledc_timer);

	ledc_channel_config(&red_channel);
	ledc_channel_config(&green_channel);
	ledc_channel_config(&blue_channel);


	while(1){
		vTaskDelay(100 / portTICK_PERIOD_MS);
		update_rgb_duty(&rgb_led);
	}
}
