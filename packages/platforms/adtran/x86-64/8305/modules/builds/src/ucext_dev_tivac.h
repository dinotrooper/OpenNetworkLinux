#ifndef _UCEXT_DEV_TIVAC_H_
#define _UCEXT_DEV_TIVAC_H_

#ifndef UCEXT_TIVAC_DEVICE_INDEX
    #define UCEXT_TIVAC_DEVICE_INDEX	 0
#endif

/* Define the default numbers of each HWMON sensor currently supported */
#ifndef NUM_UCEXT_TIVAC_CURRS
    #define NUM_UCEXT_TIVAC_CURRS    0
#endif

#ifndef NUM_UCEXT_TIVAC_FANS
    #define NUM_UCEXT_TIVAC_FANS     4
#endif

#ifndef UCEXT_TIVAC_SYSTEM_FAN
    #define UCEXT_TIVAC_SYSTEM_FAN   1
#endif

#ifndef NUM_UCEXT_TIVAC_PWMS
    #define NUM_UCEXT_TIVAC_PWMS     1
#endif

#ifndef NUM_UCEXT_TIVAC_TEMPS
    #define NUM_UCEXT_TIVAC_TEMPS    6
#endif

#ifndef NUM_UCEXT_TIVAC_VOLTS
    #define NUM_UCEXT_TIVAC_VOLTS    0
#endif

#ifndef UCEXT_TIVAC_PWRGOOD
    #define UCEXT_TIVAC_PWRGOOD      0
#endif

#ifndef UCEXT_TIVAC_VIN_ALARMS
    #define UCEXT_TIVAC_VIN_ALARMS   2
#endif

#ifndef NUM_UCEXT_TIVAC_LEDS
    #define NUM_UCEXT_TIVAC_LEDS     3
#endif

#ifndef NUM_UCEXT_TIVAC_BUTTONS
    #define NUM_UCEXT_TIVAC_BUTTONS  1
#endif

#ifndef UCEXT_MAX_LABEL_SIZE
    #define UCEXT_MAX_LABEL_SIZE	 24
#endif

#ifndef UCEXT_TIVAC_MAX_UPDATE_INTERVAL
    #define UCEXT_TIVAC_MAX_UPDATE_INTERVAL   65535
#endif

/* The minimum interval is somewhat defined by the fastest poll rate of the Tiva Firmware (currently about 200 msec/per sensor pair) */
#ifndef UCEXT_TIVAC_MIN_UPDATE_INTERVAL
    #define UCEXT_TIVAC_MIN_UPDATE_INTERVAL     2000
#endif

#ifndef UCEXT_TIVAC_DEVM_MEM_ALLOC
/* Note, non-device managed allocations are not currently working! but this flag is used to debug it */
    #define UCEXT_TIVAC_DEVM_MEM_ALLOC  1
#endif

/* TIVAC Multi-Purpose Device subsystem and data structure init and exit */
extern void *ucext_dev_tivac_init(struct device *dev);
extern void  ucext_dev_tivac_exit(struct device *dev);

#endif
