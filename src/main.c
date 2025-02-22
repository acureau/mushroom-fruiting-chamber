#include <stdio.h>
#include "pico/stdlib.h"
#include "dht22-pico.h"
#include "hardware/adc.h"

// GPIO Mapping
#define TEMP_HUMIDITY_SENSOR_GPIO 0
#define FLOAT_SWITCH_OUT_GPIO 1
#define FLOAT_SWITCH_IN_GPIO 2
#define FANS_SWITCH_GPIO 16
#define HEAT_PADS_SWITCH_GPIO 17
#define LIGHT_SWITCH_GPIO 18
#define PUMP_SWITCH_GPIO 19
#define HUMIDIFIER_SWITCH_GPIO 20

// Controller Parameters
#define MS_BETWEEN_CONTROLLER_CYCLES 500
#define MINIMUM_TEMPERATURE 70.0
#define MAXIMUM_TEMPERATURE 80.0
#define MINIMUM_HUMIDITY 80.0
#define AIR_CYCLE_INTERVAL_MINS 15.0
#define AIR_CYCLE_DURATION_MINS 2.0
#define LIGHT_CYCLE_HOURS_PER_DAY 12.0

// Temperature / humidity sensor state.
dht_reading temp_humidity_sensor;
uint64_t last_air_cycle_time;

// Light state.
uint64_t last_light_cycle_time;

// Temp / humidity sensor failed cycle count, used to ignore a safe amount of failure.
int8_t temp_humidity_sensor_fail_cycles;

// Initializes all SDK and hardware components.
void initialize() 
{
    // Delay one cycle to ensure proper initialization.
    sleep_ms(MS_BETWEEN_CONTROLLER_CYCLES);

    // Configure USB stdio.
    stdio_usb_init();

    // Configure ADC.
    adc_init();

    // Configure built-in LED GPIO settings, set on to indicate powered.
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, true);

    // Configure fans switch GPIO settings.
    gpio_init(FANS_SWITCH_GPIO);
    gpio_set_dir(FANS_SWITCH_GPIO, GPIO_OUT);
    gpio_put(FANS_SWITCH_GPIO, false);

    // Configure heat pads switch GPIO settings.
    gpio_init(HEAT_PADS_SWITCH_GPIO);
    gpio_set_dir(HEAT_PADS_SWITCH_GPIO, GPIO_OUT);
    gpio_put(HEAT_PADS_SWITCH_GPIO, false);

    // Configure light switch GPIO settings.
    gpio_init(LIGHT_SWITCH_GPIO);
    gpio_set_dir(LIGHT_SWITCH_GPIO, GPIO_OUT);
    gpio_put(LIGHT_SWITCH_GPIO, false);

    // Configure light switch GPIO settings.
    gpio_init(PUMP_SWITCH_GPIO);
    gpio_set_dir(PUMP_SWITCH_GPIO, GPIO_OUT);
    gpio_put(PUMP_SWITCH_GPIO, false);

    // Configure humidifier switch GPIO settings.
    gpio_init(HUMIDIFIER_SWITCH_GPIO);
    gpio_set_dir(HUMIDIFIER_SWITCH_GPIO, GPIO_OUT);
    gpio_put(HUMIDIFIER_SWITCH_GPIO, false);

    // Configure float switch settings.
    gpio_init(FLOAT_SWITCH_OUT_GPIO);
    gpio_init(FLOAT_SWITCH_IN_GPIO);
    gpio_set_dir(FLOAT_SWITCH_OUT_GPIO, GPIO_OUT);
    gpio_set_dir(FLOAT_SWITCH_IN_GPIO, GPIO_IN);
    gpio_put(FLOAT_SWITCH_OUT_GPIO, true);

    // Initialize temperature / humidity sensor.
    temp_humidity_sensor = dht_init(TEMP_HUMIDITY_SENSOR_GPIO);
    temp_humidity_sensor_fail_cycles = 0;

    // Initialize air flow cycle.
    last_air_cycle_time = time_us_64();

    // Initialize light cycle.
    last_light_cycle_time = time_us_64();
}

// Reads the temperature from the sensor in farenheit.
float read_temperature()
{
    return (float)((temp_humidity_sensor.temp_celsius * 9.0 / 5.0) + 32.0);
}

// Reads the humidity from the sensor as a percentage of 100.
float read_humidity()
{
    return temp_humidity_sensor.humidity;
}

// Determines if the water level is low (switch is open).
bool is_water_low()
{
    return !gpio_get(FLOAT_SWITCH_IN_GPIO);
}

// Determines if the air cycle time is active.
bool is_air_cycle_active()
{
    uint64_t current_time = time_us_64();
    float minutes_since_last_air_cycle = (float)((current_time - last_air_cycle_time) / 60000000.0f);

    // Check if it's time for an air cycle.
    if (minutes_since_last_air_cycle >= AIR_CYCLE_INTERVAL_MINS)
    {
        last_air_cycle_time = current_time;  // Reset the cycle timer
        return true;                         // Start a new air cycle
    }

    // Check if we are currently in the air cycle duration.
    if (minutes_since_last_air_cycle < AIR_CYCLE_DURATION_MINS)
    {
        return true;
    }

    // Otherwise, the air cycle is inactive.
    return false;
}

// Determines if the light cycle is active.
bool is_light_cycle_active()
{
    uint64_t current_time = time_us_64();
    float hours_since_last_light_cycle = (float)((current_time - last_light_cycle_time) / 3600000000.0f);
    if (hours_since_last_light_cycle < LIGHT_CYCLE_HOURS_PER_DAY)
    {
        return true;
    }
    else if (hours_since_last_light_cycle >= 24.0f) 
    {
        last_light_cycle_time = current_time;
    }
    return false;
}

int main() 
{
    // Initialize the system. 
    initialize();

    // Main controller cycle.
    while (true) 
    {
        // Temperature and humidity sensor health check.
        if (dht_read(&temp_humidity_sensor) == DHT_OK) 
        {
            // Reset temp / humidity sensor failure cycle count.
            temp_humidity_sensor_fail_cycles = 0;

            // Read sensor data.
            float temperature = read_temperature();
            float humidity = read_humidity();

            // Turn on fans when above maximum allowed temperature or when air cycle is active.
            // (Fans circulate air and regulate temperature.)
            bool fans_active = (temperature > MAXIMUM_TEMPERATURE || is_air_cycle_active());

            // Turn on heater when below minimum allowed temperature.
            // (Heater regulates temperature.)
            bool heat_pads_active = (temperature < MINIMUM_TEMPERATURE);

            // Turn on light when cycle is active.
            // (Light cycle influences fruiting process.)
            bool light_active = is_light_cycle_active();

            // Turn on pump when below minimum allowed water level.
            // (Pump maintains the humidifier resevoir.)
            bool pump_active = is_water_low();

            // Turn on humidifier when below minimum allowed humidity.
            // (Humidifier regulates relative humidity.)
            bool humidifier_active = (humidity < MINIMUM_HUMIDITY);

            // Set switch state.
            gpio_put(FANS_SWITCH_GPIO, fans_active);
            gpio_put(HEAT_PADS_SWITCH_GPIO, heat_pads_active);
            gpio_put(LIGHT_SWITCH_GPIO, light_active);
            gpio_put(PUMP_SWITCH_GPIO, pump_active);
            gpio_put(HUMIDIFIER_SWITCH_GPIO, humidifier_active);
            
            // Debug output.
            printf(
                "\n"
                "Current Temperature: %.2fF\n"
                "Current Humidity: %.2f%%\n"
                "Fans Active: %s\n"
                "Heat Pads Active: %s\n"
                "Light Active: %s\n"
                "Pump Active: %s\n"
                "Humidifier Active: %s\n",
                temperature, 
                humidity, 
                fans_active ? "Yes" : "No", 
                heat_pads_active ? "Yes" : "No", 
                light_active ? "Yes" : "No", 
                pump_active ? "Yes" : "No", 
                humidifier_active ? "Yes" : "No"
            );
        }

        // If check fails turn off pump.
        else
        {
            temp_humidity_sensor_fail_cycles++;
            if (temp_humidity_sensor_fail_cycles >= 20) 
            {
                printf("Temperature and humidity sensor error.\n");
                gpio_put(PICO_DEFAULT_LED_PIN, false);
                gpio_put(FANS_SWITCH_GPIO, false);
                gpio_put(HEAT_PADS_SWITCH_GPIO, false);
                gpio_put(LIGHT_SWITCH_GPIO, false);
                gpio_put(PUMP_SWITCH_GPIO, false);
                gpio_put(HUMIDIFIER_SWITCH_GPIO, false);
            }
        }

        // Wait for next cycle.
        sleep_ms(MS_BETWEEN_CONTROLLER_CYCLES);
    }

    return 0;
}