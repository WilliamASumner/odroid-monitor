#ifndef MONITOR_H
#define MONITOR_H

#define SENSOR_START 1
#define SENSOR_END 0
#define SENSOR_ENABLE( X ) ("/sys/bus/i2c/drivers/INA231/3-" #X "/enable")
#define SENSOR_W( X ) ("/sys/bus/i2c/drivers/INA231/3-" #X "/_W")
#define NUM_SENSORS 4
#define NUM_CORES 8


struct odroid_state {
    int read_fds[NUM_SENSORS]; // sensor reading file descriptors array
    int enable_fds[NUM_SENSORS]; // sensor enable file descriptors array
};

struct power_stats {
    double sensor_w[NUM_SENSORS]; // sensor wattage
};

void print_args(char **arg_list, int num_args);

void cleanup_args(char **arg_list, int num_args);

void toggle_sensors(struct odroid_state * state, int state_code);

void init_odroid_state(struct odroid_state * state);

void end_odroid_state(struct odroid_state * state);

void get_power(struct odroid_state * state);

void write_results(double data,long time);

#endif
