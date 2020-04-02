#ifndef _UCEXT_DEV_TOMAHAWK_H_
#define _UCEXT_DEV_TOMAHAWK_H_

#ifndef UCEXT_TOMAHAWK_DEVICE_INDEX
    #define UCEXT_TOMAHAWK_DEVICE_INDEX	   2
#endif

/* Define the default numbers of each HWMON sensor currently supported */
#ifndef NUM_UCEXT_TOMAHAWK_CURRS
    #define NUM_UCEXT_TOMAHAWK_CURRS    1
#endif

#ifndef NUM_UCEXT_TOMAHAWK_FANS
    #define NUM_UCEXT_TOMAHAWK_FANS     0
#endif

#ifndef UCEXT_TOMAHAWK_SYSTEM_FAN
    #define UCEXT_TOMAHAWK_SYSTEM_FAN   0
#endif

#ifndef NUM_UCEXT_TOMAHAWK_PWMS
    #define NUM_UCEXT_TOMAHAWK_PWMS     0
#endif

#ifndef NUM_UCEXT_TOMAHAWK_TEMPS
    #define NUM_UCEXT_TOMAHAWK_TEMPS    1
#endif

#ifndef NUM_UCEXT_TOMAHAWK_VOLTS
    #define NUM_UCEXT_TOMAHAWK_VOLTS    7
#endif

#ifndef UCEXT_TOMAHAWK_PWRGOOD
    #define UCEXT_TOMAHAWK_PWRGOOD      1
#endif

#ifndef UCEXT_TOMAHAWK_VIN_ALARMS
    #define UCEXT_TOMAHAWK_VIN_ALARMS   0
#endif

#ifndef NUM_UCEXT_TOMAHAWK_LEDS
    #define NUM_UCEXT_TOMAHAWK_LEDS     0
#endif

#ifndef NUM_UCEXT_TOMAHAWK_BUTTONS
    #define NUM_UCEXT_TOMAHAWK_BUTTONS  0
#endif

#ifndef UCEXT_MAX_LABEL_SIZE
    #define UCEXT_MAX_LABEL_SIZE	   24
#endif

#ifndef UCEXT_TOMAHAWK_MAX_UPDATE_INTERVAL
    #define UCEXT_TOMAHAWK_MAX_UPDATE_INTERVAL   65535
#endif

/* The minimum interval is somewhat defined by the fastest poll rate of the Tiva Firmware (currently about 200 msec/per sensor pair) */
#ifndef UCEXT_TOMAHAWK_MIN_UPDATE_INTERVAL
    #define UCEXT_TOMAHAWK_MIN_UPDATE_INTERVAL     2000
#endif

#ifndef UCEXT_TOMAHAWK_DEVM_MEM_ALLOC
/* Note, non-device managed allocations are not currently working! but this flag is used to debug it */
    #define UCEXT_TOMAHAWK_DEVM_MEM_ALLOC  1
#endif

/* TOMAHAWK Multi-Purpose Device subsystem and data structure init and exit */
extern void *ucext_dev_tomahawk_init(struct device *dev);
extern void  ucext_dev_tomahawk_exit(struct device *dev);

#endif
