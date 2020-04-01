#ifndef _UCEXT_DEV_DENVERTON_H_
#define _UCEXT_DEV_DENVERTON_H_

#ifndef UCEXT_DENVERTON_DEVICE_INDEX
    #define UCEXT_DENVERTON_DEVICE_INDEX	   3
#endif

/* Define the default numbers of each HWMON sensor currently supported */
#ifndef NUM_UCEXT_DENVERTON_CURRS
    #define NUM_UCEXT_DENVERTON_CURRS    5
#endif

#ifndef NUM_UCEXT_DENVERTON_FANS
    #define NUM_UCEXT_DENVERTON_FANS     0
#endif

#ifndef UCEXT_DENVERTON_SYSTEM_FAN
    #define UCEXT_DENVERTON_SYSTEM_FAN   0
#endif

#ifndef NUM_UCEXT_DENVERTON_PWMS
    #define NUM_UCEXT_DENVERTON_PWMS     0
#endif

#ifndef NUM_UCEXT_DENVERTON_TEMPS
    #define NUM_UCEXT_DENVERTON_TEMPS    5
#endif

#ifndef NUM_UCEXT_DENVERTON_VOLTS
    #define NUM_UCEXT_DENVERTON_VOLTS    9
#endif

#ifndef UCEXT_DENVERTON_PWRGOOD
    #define UCEXT_DENVERTON_PWRGOOD      1
#endif

#ifndef UCEXT_DENVERTON_VIN_ALARMS
    #define UCEXT_DENVERTON_VIN_ALARMS   0
#endif

#ifndef NUM_UCEXT_DENVERTON_LEDS
    #define NUM_UCEXT_DENVERTON_LEDS     0
#endif

#ifndef NUM_UCEXT_DENVERTON_BUTTONS
    #define NUM_UCEXT_DENVERTON_BUTTONS  0
#endif

#ifndef UCEXT_MAX_LABEL_SIZE
    #define UCEXT_MAX_LABEL_SIZE	   24
#endif

#ifndef UCEXT_DENVERTON_MAX_UPDATE_INTERVAL
    #define UCEXT_DENVERTON_MAX_UPDATE_INTERVAL   65535
#endif

/* The minimum interval is somewhat defined by the fastest poll rate of the Tiva Firmware (currently about 200 msec/per sensor pair) */
#ifndef UCEXT_DENVERTON_MIN_UPDATE_INTERVAL
    #define UCEXT_DENVERTON_MIN_UPDATE_INTERVAL     2000
#endif

#ifndef UCEXT_DENVERTON_DEVM_MEM_ALLOC
/* Note, non-device managed allocations are not currently working! but this flag is used to debug it */
    #define UCEXT_DENVERTON_DEVM_MEM_ALLOC  1
#endif

/* DENVERTON Multi-Purpose Device subsystem and data structure init and exit */
extern void *ucext_dev_denverton_init(struct device *dev);
extern void  ucext_dev_denverton_exit(struct device *dev);

#endif
