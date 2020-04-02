#ifndef _UCEXT_DEV_SHAREDPWR_H_
#define _UCEXT_DEV_SHAREDPWR_H_

#ifndef UCEXT_SHAREDPWR_DEVICE_INDEX
    #define UCEXT_SHAREDPWR_DEVICE_INDEX	   1
#endif

/* Define the default numbers of each HWMON sensor currently supported */
#ifndef NUM_UCEXT_SHAREDPWR_CURRS
    #define NUM_UCEXT_SHAREDPWR_CURRS    2
#endif

#ifndef NUM_UCEXT_SHAREDPWR_FANS
    #define NUM_UCEXT_SHAREDPWR_FANS     0
#endif

#ifndef UCEXT_SHAREDPWR_SYSTEM_FAN
    #define UCEXT_SHAREDPWR_SYSTEM_FAN   0
#endif

#ifndef NUM_UCEXT_SHAREDPWR_PWMS
    #define NUM_UCEXT_SHAREDPWR_PWMS     0
#endif

#ifndef NUM_UCEXT_SHAREDPWR_TEMPS
    #define NUM_UCEXT_SHAREDPWR_TEMPS    4
#endif

#ifndef NUM_UCEXT_SHAREDPWR_VOLTS
    #define NUM_UCEXT_SHAREDPWR_VOLTS    7
#endif

#ifndef UCEXT_SHAREDPWR_PWRGOOD
    #define UCEXT_SHAREDPWR_PWRGOOD      1
#endif

#ifndef UCEXT_SHAREDPWR_VIN_ALARMS
    #define UCEXT_SHAREDPWR_VIN_ALARMS   0
#endif

#ifndef NUM_UCEXT_SHAREDPWR_LEDS
    #define NUM_UCEXT_SHAREDPWR_LEDS     0
#endif

#ifndef NUM_UCEXT_SHAREDPWR_BUTTONS
    #define NUM_UCEXT_SHAREDPWR_BUTTONS  0
#endif

#ifndef UCEXT_MAX_LABEL_SIZE
    #define UCEXT_MAX_LABEL_SIZE	   24
#endif

#ifndef UCEXT_SHAREDPWR_MAX_UPDATE_INTERVAL
    #define UCEXT_SHAREDPWR_MAX_UPDATE_INTERVAL   65535
#endif

/* The minimum interval is somewhat defined by the fastest poll rate of the Tiva Firmware (currently about 200 msec/per sensor pair) */
#ifndef UCEXT_SHAREDPWR_MIN_UPDATE_INTERVAL
    #define UCEXT_SHAREDPWR_MIN_UPDATE_INTERVAL     2000
#endif

#ifndef UCEXT_SHAREDPWR_DEVM_MEM_ALLOC
/* Note, non-device managed allocations are not currently working! but this flag is used to debug it */
    #define UCEXT_SHAREDPWR_DEVM_MEM_ALLOC  1
#endif

/* SHAREDPWR Multi-Purpose Device subsystem and data structure init and exit */
extern void *ucext_dev_sharedpwr_init(struct device *dev);
extern void  ucext_dev_sharedpwr_exit(struct device *dev);

#endif
