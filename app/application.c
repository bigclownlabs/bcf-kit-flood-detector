#include <application.h>

#define FIRMWARE "bcf-kit-flood-detector:" TAG

#define BATTERY_UPDATE_INTERVAL (60 * 60 * 1000)
#define TEMPERATURE_PUB_NO_CHANGE_INTEVAL (15 * 60 * 1000)
#define TEMPERATURE_PUB_VALUE_CHANGE 0.2f
#define TEMPERATURE_UPDATE_INTERVAL (1 * 1000)

#define FLOOD_DETECTOR_NO_CHANGE_INTEVAL (15 * 60 * 1000)
#define FLOOD_DETECTOR_UPDATE_INTERVAL (1 * 1000)

#define RADIO_FLOOD_DETECTOR        0x0d

// LED instance
bc_led_t led;

// Button instance
bc_button_t button;

// Temperature instance
bc_tag_temperature_t temperature;
event_param_t temperature_event_param = {
        .number =  (BC_I2C_I2C0 << 7) | BC_TAG_TEMPERATURE_I2C_ADDRESS_ALTERNATE,
        .next_pub = 0
};

bc_flood_detector_t flood_detector;
event_param_t flood_detector_event_param = {
        .number = 'a',
        .next_pub = 0
};

void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param)
{
    (void) event_param;

    if (event == BC_BUTTON_EVENT_PRESS)
    {
        bc_led_pulse(&led, 100);
    }
}

void battery_event_handler(bc_module_battery_event_t event, void *event_param)
{
    (void) event;
    (void) event_param;

    float voltage;

    if (bc_module_battery_get_voltage(&voltage))
    {
        bc_radio_pub_battery(BC_MODULE_BATTERY_FORMAT_MINI, &voltage);
    }
}

void temperature_tag_event_handler(bc_tag_temperature_t *self, bc_tag_temperature_event_t event, void *event_param)
{
    float value;
    event_param_t *param = (event_param_t *)event_param;

    if (event == BC_TAG_TEMPERATURE_EVENT_UPDATE)
    {
        if (bc_tag_temperature_get_temperature_celsius(self, &value))
        {
            if ((fabs(value - param->value) >= TEMPERATURE_PUB_VALUE_CHANGE) || (param->next_pub < bc_scheduler_get_spin_tick()))
            {
                bc_radio_pub_thermometer(param->number, &value);

                param->value = value;
                param->next_pub = bc_scheduler_get_spin_tick() + TEMPERATURE_PUB_NO_CHANGE_INTEVAL;
            }
        }
    }
}

void flood_detector_event_handler(bc_flood_detector_t *self, bc_flood_detector_event_t event, void *event_param)
{
    bool is_alarm;
    event_param_t *param = (event_param_t *)event_param;


    if (event == BC_FLOOD_DETECTOR_EVENT_UPDATE)
    {
       is_alarm = bc_flood_detector_is_alarm(self);

       if ((is_alarm != param->value) || (param->next_pub < bc_scheduler_get_spin_tick()))
       {
           uint8_t buffer[3] = { RADIO_FLOOD_DETECTOR, 'a', is_alarm };

           bc_radio_pub_buffer(buffer, sizeof(buffer));

           param->value = is_alarm;
           param->next_pub = bc_scheduler_get_spin_tick() + FLOOD_DETECTOR_NO_CHANGE_INTEVAL;
       }
    }
}

void application_init(void)
{
    // Initialize LED
    bc_led_init(&led, BC_GPIO_LED, false, false);
    bc_led_set_mode(&led, BC_LED_MODE_OFF);

    bc_radio_init();

    // Initialize button
    bc_button_init(&button, BC_GPIO_BUTTON, BC_GPIO_PULL_DOWN, false);
    bc_button_set_scan_interval(&button, 20);
    bc_button_set_event_handler(&button, button_event_handler, NULL);

    // Initialize battery
    bc_module_battery_init(BC_MODULE_BATTERY_FORMAT_MINI);
    bc_module_battery_set_event_handler(battery_event_handler, NULL);
    bc_module_battery_set_update_interval(BATTERY_UPDATE_INTERVAL);

    // Initialize temperature
    bc_tag_temperature_init(&temperature, BC_I2C_I2C0, BC_TAG_TEMPERATURE_I2C_ADDRESS_ALTERNATE);
    bc_tag_temperature_set_update_interval(&temperature, TEMPERATURE_UPDATE_INTERVAL);
    bc_tag_temperature_set_event_handler(&temperature, temperature_tag_event_handler, &temperature_event_param);

    // Initialize flood detector
    bc_flood_detector_init(&flood_detector, BC_FLOOD_DETECTOR_TYPE_LD_81_SENSOR_MODULE_CHANNEL_A);
    bc_flood_detector_set_event_handler(&flood_detector, flood_detector_event_handler, &flood_detector_event_param);
    bc_flood_detector_set_update_interval(&flood_detector, FLOOD_DETECTOR_UPDATE_INTERVAL);

    bc_radio_enroll_to_gateway();

    bc_radio_pub_info(FIRMWARE);

    bc_led_pulse(&led, 2000);
}
