/*
 * Copyright (C) 2019 Martine Lenders <mail@martine-lenders.eu
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @file
 * @brief       A wand for CoWN4
 *
 * @author      Martine Lenders <mail@martine-lenders.eu>
 */

#include <stdio.h>
#include <time.h>

#include "periph/adc.h"
#include "periph/gpio.h"
#include "periph/rtc.h"
#include "periph/timer.h"

#define TIMER_FREQ          (1000000UL)             /**< Timer frequency */

/**
 * @name Battery ADC of Feather M0
 * @{
 */
#ifndef BATTERY
#define BATTERY             (ADC_LINE(7))
#endif
#ifndef BATTERY_RES
#define BATTERY_RES         (ADC_RES_10BIT)
#endif
/** @} */

/**
 * @name Time spell LEDs
 *
 * The colors refer to the wiring in my prototype
 *
 * @{
 */
/* Green LEDs for minutes ("half-quarter" hours) */
#ifndef LED_MINUTE_YELLOW
#define LED_MINUTE_YELLOW   (GPIO_PIN(PA, 3))       /* A0 on Feather-M0 */
#endif
#ifndef LED_MINUTE_GREEN
#define LED_MINUTE_GREEN    (GPIO_PIN(PA, 8))       /* A1 on Feather-M0 */
#endif
#ifndef LED_MINUTE_BLUE
#define LED_MINUTE_BLUE     (GPIO_PIN(PA, 9))       /* A2 on Feather-M0 */
#endif
#ifndef LED_MINUTE_GRAY
#define LED_MINUTE_GRAY     (GPIO_PIN(PA, 4))       /* A3 on Feather-M0 */
#endif
/* Red LEDs for hours */
#ifndef LED_HOUR_BROWN
#define LED_HOUR_BROWN      (GPIO_PIN(PA, 5))       /* A4 on Feather-M0 */
#endif
#ifndef LED_HOUR_PURPLE
#define LED_HOUR_PURPLE     (GPIO_PIN(PB, 2))       /* A5 on Feather-M0 */
#endif
#ifndef LED_HOUR_RED
#define LED_HOUR_RED        (GPIO_PIN(PA, 11))      /* SCK on Feather-M0 */
#endif
#ifndef LED_HOUR_ORANGE
#define LED_HOUR_ORANGE     (GPIO_PIN(PA, 10))      /* MOSI on Feather-M0 */
#endif
/** @} */

/**
 * @brief Light spell LED
 */
#ifndef LED_LIGHT
#define LED_LIGHT           (GPIO_PIN(PA, 12))      /* MISO on Feather-M0 */
#endif

/**
 * @brief Button to activate time spell
 */
#ifndef BTN_TIME
#define BTN_TIME            (GPIO_PIN(PA, 23))      /* SCL on Feather-M0 */
#endif

/**
 * @brief Button to activate light spell
 */
#ifndef BTN_LIGHT
#define BTN_LIGHT           (GPIO_PIN(PA, 22))      /* SDA on Feather-M0 */
#endif

#define TIME_LEDS_QTRS_IDX  (3U)                    /**< last index of quarters */
#define TIME_LEDS_NUMOF     (8U)                    /**< number of time spell LEDs */
#define HOUR_MAX            (12U)                   /**< max. number of hours */
#define QUARTER_MAX         (8U)                    /**< max. numbers of (half) quarters */

#define BLINK_TIMEOUT       (500000UL)              /**< timeout for "timer set mode" blinking */

#define POWER_BAR_CHANNEL   (0)                     /**< channel to timeout "power bar event" on */
#define POWER_BAR_TIMEOUT   (1U * TIMER_FREQ)       /**< timeout for "power bar event" */

/**
 * @brief   channel to timeout activation of "set time mode" on
 */
#define SET_TIME_CHANNEL    (1)

/**
 * @brief   timeout after which to activate "set time mode"
 */
#define SET_TIME_TIMEOUT    (3U * TIMER_FREQ)

/**
 * @brief   Minimum voltage on @ref BATTERY
 */
#define VOLTAGE_MIN         (3276)

/**
 * @brief   Maximum voltage on @ref BATTERY
 */
#define VOLTAGE_MAX         (4301)

static const gpio_t _time_array[] = {
    LED_MINUTE_YELLOW,
    LED_MINUTE_GREEN,
    LED_MINUTE_BLUE,
    LED_MINUTE_GRAY,
    LED_HOUR_BROWN,
    LED_HOUR_PURPLE,
    LED_HOUR_RED,
    LED_HOUR_ORANGE,
};

typedef enum {
    SET_TIME_MODE_OFF = 0,
    SET_TIME_MODE_QUARTERS = 4,
    SET_TIME_MODE_HOURS = 8,
} _set_time_mode_t;

static int _lights_on;
static int _blinking;
static _set_time_mode_t _set_time_mode;
struct tm _time;
static kernel_pid_t _main_pid;
static uint8_t _cur_hour;
static uint8_t _cur_quarter;

/**
 * @name Functions and handlers for "normal mode"
 */
/**
 * @brief   Switch to normal mode
 */
static void _init_normal_mode(void);
/**
 * @brief   GPIO handler for pushing @ref BTN_LIGHT
 *
 * @param[in] arg   Argument for GPIO handler
 */
static void _btn_light_push(void *arg);
/**
 * @brief   GPIO handler for releasing @ref BTN_LIGHT
 *
 * @param[in] arg   Argument for GPIO handler
 */
static void _btn_light_release(void *arg);
/**
 * @brief   GPIO handler for pushing @ref BTN_TIME
 *
 * @param[in] arg   Argument for GPIO handler
 */
static void _btn_time_push(void *arg);
/**
 * @brief   GPIO handler for releasing @ref BTN_TIME
 *
 * @param[in] arg   Argument for GPIO handler
 */
static void _btn_time_release(void *arg);
/**
 * @brief   Handler for "power bar event"
 *
 * Shows the power supply on time LEDs as a "bar".
 */
static void _show_power(void);
/**
 * @brief   Encodes current time as LED sequence
 *
 * @param[in] hour      The current hour
 * @param[in] quarter   The current "half" quarter hour
 *
 * @p hour is converted to binary.
 * @p quarter into "half quarters":
 * 00 => LED 1, 15 => LED 2, 30 => LED 3, 45 => LED 4
 * 07 => LEDs 1+2, 22 => LED 2+3, 37 => LED 3+4, 52 => LED 4+1
 *
 * @return  Bit-encoded LED-map
 */
static uint8_t _time_encode(uint8_t hour, uint8_t quarter);
/**
 * @brief   Shows the time either static or blinking
 *
 * @param[in] gpio_show gpio_set for static, gpio_toggle for blinking
 */
static void _show_time(void (*gpio_show)(gpio_t));
/**
 * @brief   Turns off all time LEDs
 */
static void _turn_off_time_display(void);
/**
 * @brief   Get current time from RTC
 */
static void _time_get(void);
/** @} */

/**
 * @name Functions and handlers for "set time mode"
 */
/**
 * @brief   Switch to set timer mode
 */
static void _init_set_time_mode(void);
/**
 * @brief   GPIO handler for pushing @ref BTN_LIGHT in "set time mode"
 *
 * @param[in] arg   Argument for GPIO handler
 */
static void _decrement(void *arg);
/**
 * @brief   GPIO handler for releasing @ref BTN_LIGHT in "set time mode"
 *
 * @param[in] arg   Argument for GPIO handler
 */
static void _do_decrement(void *arg);
/**
 * @brief   GPIO handler for pushing @ref BTN_TIME in "set time mode"
 *
 * @param[in] arg   Argument for GPIO handler
 */
static void _increment(void *arg);
/**
 * @brief   GPIO handler for releasing @ref BTN_TIME in "set time mode"
 *
 * @param[in] arg   Argument for GPIO handler
 */
static void _do_increment(void *arg);
/**
 * @brief   One half period of blinking LEDs
 */
static void _blink(void);
/**
 * @brief   Return to blinking time LEDs for
 */
static void _return_to_blink(void);
/**
 * @brief   Set current time to RTC
 */
static void _time_set(void);
/** @} */

/**
 * @brief   Timeout handler for timer
 */
static void _timeout(void *arg, int channel);

int main(void)
{
    _main_pid = sched_active_pid;
    rtc_init();
    adc_init(BATTERY);
    gpio_init(LED_LIGHT, GPIO_OUT);
    timer_init(TIMER_DEV(1), TIMER_FREQ, _timeout, NULL);
    _init_normal_mode();
    return 0;
}

static void _init_normal_mode(void)
{
    /* stop timer to safe power */
    timer_stop(TIMER_DEV(1));
    /* deactivate "set time mode" */
    _set_time_mode = SET_TIME_MODE_OFF;
    /* also stop the LED blinking */
    _blinking = 0;
    /* clear timer on both channels to prevent accidental firing later */
    timer_clear(TIMER_DEV(1), POWER_BAR_CHANNEL);
    timer_clear(TIMER_DEV(1), SET_TIME_CHANNEL);
    /* initialize buttons for normal mode */
    gpio_init_int(BTN_LIGHT, GPIO_IN_PU, GPIO_FALLING, _btn_light_push, NULL);
    gpio_init_int(BTN_TIME, GPIO_IN_PU, GPIO_FALLING, _btn_time_push, NULL);
    _turn_off_time_display();
    /* set light LED according to current status */
    if (_lights_on) {
        gpio_set(LED_LIGHT);
    }
    else {
        gpio_clear(LED_LIGHT);
    }
}

static void _btn_light_push(void *arg)
{
    (void)arg;
    /* toggle light LED status */
    _lights_on = !_lights_on;
    /* re-register button for release */
    gpio_init_int(BTN_LIGHT, GPIO_IN_PU, GPIO_RISING, _btn_light_release, NULL);
    timer_start(TIMER_DEV(1));
    /* set timer for when button is pushed for > POWER_BAR_TIMEOUT to
     * show power status */
    timer_set(TIMER_DEV(1), POWER_BAR_CHANNEL, POWER_BAR_TIMEOUT);
}

static void _btn_light_release(void *arg)
{
    (void)arg;
    /* cancel timer for power status display */
    timer_clear(TIMER_DEV(1), POWER_BAR_CHANNEL);
    /* stop timer to safe power */
    timer_stop(TIMER_DEV(1));
    /* set light LED according to current status */
    if (_lights_on) {
        gpio_set(LED_LIGHT);
    }
    else {
        gpio_clear(LED_LIGHT);
    }
    /* re-register button for pushing */
    gpio_init_int(BTN_LIGHT, GPIO_IN_PU, GPIO_FALLING, _btn_light_push, NULL);
    /* turn of all time LEDs */
}

static void _btn_time_push(void *arg)
{
    (void)arg;
    _turn_off_time_display();
    /* get time from RTC */
    _time_get();
    /* and show it on the time LEDs (statically) */
    _show_time(gpio_set);
    /* re-register button for release */
    gpio_init_int(BTN_TIME, GPIO_IN_PU, GPIO_RISING, _btn_time_release, NULL);
    timer_start(TIMER_DEV(1));
    /* set timer for when button is pushed for > SET_TIME_TIMEOUT to
     * enter "set time mode" */
    timer_set(TIMER_DEV(1), SET_TIME_CHANNEL, SET_TIME_TIMEOUT);
}

static void _btn_time_release(void *arg)
{
    (void)arg;
    /* cancel timer to enter "set time mode" */
    timer_clear(TIMER_DEV(1), SET_TIME_CHANNEL);
    _turn_off_time_display();
    /* re-register button for pushing */
    gpio_init_int(BTN_TIME, GPIO_IN_PU, GPIO_FALLING, _btn_time_push, NULL);
}

static void _show_power(void)
{
    int permillage;
    /* get battery voltage */
    int batv = adc_sample(BATTERY, BATTERY_RES);

    if (batv < 0) {
        /* just show full voltage on error */
        batv = VOLTAGE_MAX;
    }
    else {
        batv *= (2 * 3.3);
    }
    if (batv <= VOLTAGE_MIN) {
        /* show empty */
        permillage = 0U;
    }
    else if (batv > VOLTAGE_MAX) {
        /* show full */
        permillage = 1001U;
    }
    else {
        /* get permille to have better resolution below */
        permillage = (((batv - VOLTAGE_MIN) * 1000) /
                      (VOLTAGE_MAX - VOLTAGE_MIN));
    }
    /* turn on LEDs according to permillage; use red LEDs for lower 50% */
    if (permillage >= 125) {
        gpio_set(_time_array[4]);
    }
    if (permillage >= 250) {
        gpio_set(_time_array[5]);
    }
    if (permillage >= 375) {
        gpio_set(_time_array[6]);
    }
    if (permillage >= 500) {
        gpio_set(_time_array[7]);
    }
    if (permillage >= 625) {
        gpio_set(_time_array[0]);
    }
    if (permillage >= 750) {
        gpio_set(_time_array[1]);
    }
    if (permillage >= 875) {
        gpio_set(_time_array[2]);
    }
    if (permillage >= 1000) {
        gpio_set(_time_array[3]);
    }
}

static uint8_t _time_encode(uint8_t hour, uint8_t quarter)
{
    uint8_t res;

    if (!hour) {
        hour = HOUR_MAX;
    }
    res = (hour << 4);
    if (quarter == (QUARTER_MAX - 1)) {
        res |= 0x9;
    }
    else if (quarter % 2) {
        res |= (0x3 << (quarter / 2));
    }
    else {
        res |= (0x1 << (quarter / 2));
    }
    return res;
}

static void _show_time(void (*gpio_show)(gpio_t))
{
    uint8_t mask = _time_encode(_cur_hour, _cur_quarter);
    void (*gpio_current_led)(gpio_t);

    for (unsigned i = 0; i < TIME_LEDS_NUMOF; i++) {
        /* when hours are set, only blink hour LEDs;
         * when quarters are set, only blink quarter LEDs */
        if (((_set_time_mode == SET_TIME_MODE_QUARTERS) &&
             (i <= TIME_LEDS_QTRS_IDX)) ||
            ((_set_time_mode == SET_TIME_MODE_HOURS) &&
             (i > TIME_LEDS_QTRS_IDX))) {
            gpio_current_led = gpio_show;
        }
        else {
            /* the rest is shown statically */
            gpio_current_led = gpio_set;
        }
        if (mask & 0x1) {
            gpio_current_led(_time_array[i]);
        }
        mask >>= 1;
    }
}

static void _turn_off_time_display(void)
{
    for (unsigned i = 0; i < TIME_LEDS_NUMOF; i++) {
        gpio_clear(_time_array[i]);
    }
}

static void _time_get(void)
{
    rtc_get_time(&_time);
    _cur_hour = _time.tm_hour % HOUR_MAX;
    _cur_quarter = (_time.tm_min / (60 / QUARTER_MAX));
}

static inline int _in_set_time_mode(void)
{
    return (_set_time_mode != SET_TIME_MODE_OFF);
}

static void _init_set_time_mode(void)
{
    gpio_init_int(BTN_LIGHT, GPIO_IN_PU, GPIO_FALLING, _decrement, NULL);
    gpio_init_int(BTN_TIME, GPIO_IN_PU, GPIO_FALLING, _increment, NULL);
    _set_time_mode = SET_TIME_MODE_HOURS;
    _return_to_blink();
}

static void _decrement(void *arg)
{
    (void)arg;
    gpio_init_int(BTN_LIGHT, GPIO_IN_PU, GPIO_RISING, _do_decrement, NULL);
    _show_time(gpio_set);
    _blinking = 0;
    timer_clear(TIMER_DEV(1), 0);
    timer_set(TIMER_DEV(1), POWER_BAR_CHANNEL, SET_TIME_TIMEOUT);
}

static void _do_decrement(void *arg)
{
    (void)arg;
    timer_clear(TIMER_DEV(1), POWER_BAR_CHANNEL);
    gpio_init_int(BTN_LIGHT, GPIO_IN_PU, GPIO_FALLING, _decrement, NULL);
    switch (_set_time_mode) {
        case SET_TIME_MODE_HOURS:
            if (_cur_hour > 0) {
                _cur_hour--;
            }
            else {
                _cur_hour = HOUR_MAX - 1;
            }
            break;
        case SET_TIME_MODE_QUARTERS:
            if (_cur_quarter > 0) {
                _cur_quarter--;
            }
            else {
                _cur_quarter = QUARTER_MAX - 1;
            }
            break;
        default:
            _init_normal_mode();
            break;
    }
    _return_to_blink();
}

static void _increment(void *arg)
{
    (void)arg;
    gpio_init_int(BTN_TIME, GPIO_IN_PU, GPIO_RISING, _do_increment, NULL);
    _show_time(gpio_set);
    _blinking = 0;
    timer_clear(TIMER_DEV(1), 0);
    timer_set(TIMER_DEV(1), SET_TIME_CHANNEL, SET_TIME_TIMEOUT);
}

static void _do_increment(void *arg)
{
    (void)arg;
    timer_clear(TIMER_DEV(1), SET_TIME_CHANNEL);
    gpio_init_int(BTN_TIME, GPIO_IN_PU, GPIO_FALLING, _increment, NULL);
    switch (_set_time_mode) {
        case SET_TIME_MODE_HOURS:
            _cur_hour = (_cur_hour + 1) % HOUR_MAX;
            break;
        case SET_TIME_MODE_QUARTERS:
            _cur_quarter = (_cur_quarter + 1) % QUARTER_MAX;
            break;
        default:
            _init_normal_mode();
            break;
    }
    _return_to_blink();
}

static void _blink(void)
{
    _show_time(gpio_toggle);
    timer_set(TIMER_DEV(1), 0, BLINK_TIMEOUT);
}

static void _return_to_blink(void)
{
    timer_clear(TIMER_DEV(0), POWER_BAR_CHANNEL);
    timer_clear(TIMER_DEV(0), SET_TIME_CHANNEL);
    _turn_off_time_display();
    _blinking = 1;
    timer_start(TIMER_DEV(1));
    _blink();
}

static void _timeout(void *arg, int channel)
{
    (void)arg;

    if (_blinking) {
        _blink();
        return;
    }
    switch (channel) {
        case POWER_BAR_CHANNEL:
            if (_in_set_time_mode()) {
                _init_normal_mode();
            }
            else {
                _show_power();
            }
            /* keep light as it was */
            _lights_on = !_lights_on;
            timer_stop(TIMER_DEV(1));
            break;
        case SET_TIME_CHANNEL:
            gpio_init_int(BTN_LIGHT, GPIO_IN_PU, GPIO_FALLING, _decrement,
                          NULL);
            gpio_init_int(BTN_TIME, GPIO_IN_PU, GPIO_FALLING, _increment,
                          NULL);
            _return_to_blink();
            switch (_set_time_mode) {
                case SET_TIME_MODE_OFF:
                    _init_set_time_mode();
                    break;
                case SET_TIME_MODE_HOURS:
                    _set_time_mode = SET_TIME_MODE_QUARTERS;
                    break;
                case SET_TIME_MODE_QUARTERS:
                    _time_set();
                    /* intentionally falls through */
                default:
                    _init_normal_mode();
                    break;
            }
            break;
    }
}

static void _time_set(void)
{
    _time.tm_hour = _cur_hour;
    _time.tm_min = _cur_quarter * (60 / QUARTER_MAX);
    _time.tm_sec = 0;

    rtc_set_time(&_time);
}
