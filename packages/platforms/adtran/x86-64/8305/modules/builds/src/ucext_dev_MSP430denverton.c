#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/sysfs.h>

#include "ucext_dev_MSP430denverton.h"
#include "ucext_msg.h"
#include "ucext_usb.h"

/*------------------------------------------------------------------------------------------------------------------*/
/* Sensor Labels attribute structure for HWMON                                                                                  */
/*------------------------------------------------------------------------------------------------------------------*/
struct sensors_label {
    struct device_attribute dev_attr_label;
    char attr_name[UCEXT_MAX_LABEL_SIZE];
    char attr_label[UCEXT_MAX_LABEL_SIZE];
    __u8  attr_wrcnt;    
};
#define to_label_attr(_attr) \
	container_of(_attr, struct sensors_label, dev_attr_label)

/*------------------------------------------------------------------------------------------------------------------*/
/* Private DENVERTON Device Data Context                                                                                */
/*------------------------------------------------------------------------------------------------------------------*/
struct denverton_data {
    char valid;                 /* !=0 if following fields are valid */

    struct device *dev;
    struct device *dev_hwmon;

    struct mutex update_lock;      
    unsigned long last_updated;    /* In jiffies */

    /* Minimum interval between input updates from the device (msecs).  This is more of a driver behavior item */
    int chip_update_interval; 

#if defined(UCEXT_DENVERTON_DEVICE_INDEX) && (UCEXT_DENVERTON_DEVICE_INDEX==0)
    int chip_watchdog;                                          
    int chip_reboot;                                          
#endif

    /*------- Note, order matters on these due to config storage !!!!!--------- */
    /* MSP430 Aux devices have more limited config information  */                           
    int chip_fw_major;                                          
    int chip_fw_minor;
#if defined(UCEXT_DENVERTON_DEVICE_INDEX) && (UCEXT_DENVERTON_DEVICE_INDEX==0)
    int chip_boot_major;                                          
    int chip_boot_minor;                                          
#endif
    int chip_protocol;                                          
    int chip_hw_id;                                          
#if defined(UCEXT_DENVERTON_DEVICE_INDEX) && (UCEXT_DENVERTON_DEVICE_INDEX==0)
    int chip_num_pwms;       // Currently not exposed to hwmon     
    int chip_num_fans;       // Currently not exposed to hwmon
    int chip_num_temps;      // Currently not exposed to hwmon
    int chip_num_auxilary;   // Currently not exposed to hwmon
    int chip_num_leds;       // Currently not exposed to hwmon
    int chip_clk1_major;     // Main ClkGen version (from version cmd)
    int chip_clk1_minor;
    int chip_reset_cause;    // reset cause command follows config (from reset_cause cmd)
#endif
    /*------ End order matters config section ----------------------------------*/

    /*------- Note, order matters on these due to status storage !!!!!--------- */
    /* MSP430 Aux devices have more limited status information  */                           
    int chip_dev_status;
#if defined(UCEXT_DENVERTON_DEVICE_INDEX) && (UCEXT_DENVERTON_DEVICE_INDEX==0)
    int chip_aux_dev1;
    int chip_aux_dev2;
    int chip_aux_dev3;
    int chip_aux_dev4;
    int chip_aux_dev5;
    int chip_aux_dev6;
    int chip_aux_clk1;
    int chip_aux_prog1;
    int chip_aux_therm1;
    int chip_aux_therm2;
    int chip_aux_therm3;
    int chip_aux_therm4;
    int chip_aux_therm5;
    int chip_aux_therm6;
    int chip_aux_therm7;
    int chip_aux_therm8;
    int chip_aux_therm9;
    int chip_aux_therm10;
    unsigned int chip_dev_rxpkts;
    unsigned int chip_dev_txpkts;
#endif
    /*------ End order matters status section ----------------------------------*/

    /* Current sensor measures (mA) */
#if defined(NUM_UCEXT_DENVERTON_CURRS) && (NUM_UCEXT_DENVERTON_CURRS>0)
    int curr_in_vals[NUM_UCEXT_DENVERTON_CURRS];
#else
    int curr_in_vals[1];
#endif
    /* Temperature sensor measures (milli degrees C) */
#if defined(NUM_UCEXT_DENVERTON_TEMPS) && (NUM_UCEXT_DENVERTON_TEMPS>0)
    int temp_in_vals[NUM_UCEXT_DENVERTON_TEMPS];
    int temp_mins[NUM_UCEXT_DENVERTON_TEMPS];
    int temp_min_hysts[NUM_UCEXT_DENVERTON_TEMPS];
    int temp_min_alarms[NUM_UCEXT_DENVERTON_TEMPS];
    int temp_maxs[NUM_UCEXT_DENVERTON_TEMPS];
    int temp_max_hysts[NUM_UCEXT_DENVERTON_TEMPS];
    int temp_max_alarms[NUM_UCEXT_DENVERTON_TEMPS];
    int temp_crits[NUM_UCEXT_DENVERTON_TEMPS];
    int temp_crit_hysts[NUM_UCEXT_DENVERTON_TEMPS];
    int temp_crit_alarms[NUM_UCEXT_DENVERTON_TEMPS];
    int temp_emergs[NUM_UCEXT_DENVERTON_TEMPS];
    int temp_emerg_hysts[NUM_UCEXT_DENVERTON_TEMPS];
    int temp_emerg_alarms[NUM_UCEXT_DENVERTON_TEMPS];
#else
    int temp_in_vals[1];
    int temp_mins[1];
    int temp_min_hysts[1];
    int temp_min_alarms[1];
    int temp_maxs[1];
    int temp_max_hysts[1];
    int temp_max_alarms[1];
    int temp_crits[1];
    int temp_crit_hysts[1];
    int temp_crit_alarms[1];
    int temp_emergs[1];
    int temp_emerg_hysts[1];
    int temp_emerg_alarms[1];
#endif
    /* Fan speed sensor measures (RPM) */
#if defined(NUM_UCEXT_DENVERTON_FANS) && (NUM_UCEXT_DENVERTON_FANS>0)
    int fan_in_vals[NUM_UCEXT_DENVERTON_FANS];
    int fan_present[NUM_UCEXT_DENVERTON_FANS];
    int fan_alarms[NUM_UCEXT_DENVERTON_FANS];
    int fan_faults[NUM_UCEXT_DENVERTON_FANS];
    unsigned int fan_test_alarm_bits;
    unsigned int fan_test_fault_bits;
#else
    int fan_in_vals[1];
    int fan_present[1];
    int fan_alarms[1];
    int fan_faults[1];
    int fan_avg;
    unsigned int fan_test_alarm_bits;
    unsigned int fan_test_fault_bits;
#endif

#if defined(UCEXT_DENVERTON_SYSTEM_FAN) && (UCEXT_DENVERTON_SYSTEM_FAN!=0)
    int system_fan_avg;
    int system_fan_alarm;
    int system_fan_fault;
#endif 

    /* PWM Fan speed settings 0-255 linear duty cycle */
    /* PWM Fan enables 0 = off, 1=manual, 2=auto      */
#if defined(NUM_UCEXT_DENVERTON_PWMS) && (NUM_UCEXT_DENVERTON_PWMS>0)
    int pwm_in_vals[NUM_UCEXT_DENVERTON_PWMS];
    int pwm_en_vals[NUM_UCEXT_DENVERTON_PWMS];
#else
    int pwm_in_vals[1];
    int pwm_en_vals[1];
#endif
    /* Voltage sensor measures (mV) */
#if defined(NUM_UCEXT_DENVERTON_VOLTS) && (NUM_UCEXT_DENVERTON_VOLTS>0)
    int volt_in_vals[NUM_UCEXT_DENVERTON_VOLTS];
    int volt_maxs[NUM_UCEXT_DENVERTON_VOLTS];
    int volt_mins[NUM_UCEXT_DENVERTON_VOLTS];
    int volt_lcrits[NUM_UCEXT_DENVERTON_VOLTS];
    int volt_max_alarms[NUM_UCEXT_DENVERTON_VOLTS];
    int volt_min_alarms[NUM_UCEXT_DENVERTON_VOLTS];
    int volt_lcrit_alarms[NUM_UCEXT_DENVERTON_VOLTS];
    unsigned int pwr_good_bits;
    unsigned int pwr_test_bits;
#else
    int volt_in_vals[1];
    int volt_maxs[1];
    int volt_mins[1];
    int volt_lcrits[1];
    int volt_max_alarms[1];
    int volt_min_alarms[1];
    int volt_lcrit_alarms[1];
    unsigned int pwr_good_bits;
    unsigned int pwr_test_bits;
#endif

    /* LED Color Settings 0-Off, 1-Red, 2-Green, 3-Orange   */
    /* LED Blink Rate Settings 0-Off, 1-Low, 2-Med, 3-Fast  */
#if defined(NUM_UCEXT_DENVERTON_LEDS) && (NUM_UCEXT_DENVERTON_LEDS>0)
    int led_color_vals[NUM_UCEXT_DENVERTON_LEDS];
    int led_rate_vals[NUM_UCEXT_DENVERTON_LEDS];
#else
    int led_color_vals[1];
    int led_rate_vals[1];
#endif
    /* Buttons presses */
#if defined(NUM_UCEXT_DENVERTON_BUTTONS) && (NUM_UCEXT_DENVERTON_BUTTONS>0)
    int button_vals[NUM_UCEXT_DENVERTON_VOLTS];
#else
    int button_vals[1];
#endif

    /* Pointer to dynamically allocated sensor label context */
    struct sensors_label *p_sensor_label_data;

    /* Pointers to store dynamically allocated attributes and groups associated with sensor labels */
    struct attribute **p_sensor_label_attrs;
    struct attribute_group label_group;
    const struct attribute_group *denverton_attr_groups[16];
};


/* Prototype the update device function implemented in file*/
int ucext_dev_denverton_update(struct device *dev, struct denverton_data *p_denverton_data);

/*------------------------------------------------------------------------------------------------------------------*/
/* Declare the standard supported HWMON Attributes to be registered by type "info"                                  */
/*------------------------------------------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------------------------------------------*/
/* Construct the dynamic length HWMON Current config Attributes with null termination using GNU C Array
   Initializer construct */
#if defined(NUM_UCEXT_DENVERTON_CURRS) && (NUM_UCEXT_DENVERTON_CURRS>0)
const u32 denverton_curr_config[NUM_UCEXT_DENVERTON_CURRS+1] = { [0 ... (NUM_UCEXT_DENVERTON_CURRS-1)] = (HWMON_C_INPUT), [NUM_UCEXT_DENVERTON_CURRS] = 0}; 

/* Declare the Standard HWMON Current Channel Info Structure */
const struct hwmon_channel_info denverton_curr = {
	.type = hwmon_curr,
	.config = denverton_curr_config,
};
#endif

/*------------------------------------------------------------------------------------------------------------------*/
/* Construct the dynamic length HWMON Temperature config Attributes with null termination using GNU C Array
   Initializer construct */
#if defined(NUM_UCEXT_DENVERTON_TEMPS) && (NUM_UCEXT_DENVERTON_TEMPS>0)
const u32 denverton_temp_config[NUM_UCEXT_DENVERTON_TEMPS+1] = { [0 ... (NUM_UCEXT_DENVERTON_TEMPS-1)] = 
      (HWMON_T_INPUT|HWMON_T_LABEL|
       HWMON_T_EMERGENCY|HWMON_T_EMERGENCY_HYST|HWMON_T_EMERGENCY_ALARM|
       HWMON_T_CRIT|HWMON_T_CRIT_HYST|HWMON_T_CRIT_ALARM|
       HWMON_T_MAX|HWMON_T_MAX_HYST|HWMON_T_MAX_ALARM|
       HWMON_T_MIN|HWMON_T_MIN_HYST|HWMON_T_MIN_ALARM ), [NUM_UCEXT_DENVERTON_TEMPS] = 0}; 

/* Declare the Standard HWMON Temperature Channel Info Structure */
const struct hwmon_channel_info denverton_temp = {
	.type = hwmon_temp,
	.config = denverton_temp_config,
};
#endif

/*------------------------------------------------------------------------------------------------------------------*/
/* Construct the dynamic length HWMON Fan config Attributes with null termination using GNU C Array
   Initializer construct */
#if defined(NUM_UCEXT_DENVERTON_FANS) && (NUM_UCEXT_DENVERTON_FANS>0)
const u32 denverton_fan_config[NUM_UCEXT_DENVERTON_FANS+1] = { [0 ... (NUM_UCEXT_DENVERTON_FANS-1)] = (HWMON_F_INPUT|HWMON_F_ALARM|HWMON_F_FAULT), [NUM_UCEXT_DENVERTON_FANS] = 0}; 

/* Declare the HWMON Fan Channel Info Structure */
const struct hwmon_channel_info denverton_fan = {
	.type = hwmon_fan,
	.config = denverton_fan_config,
};
#endif

/*------------------------------------------------------------------------------------------------------------------*/
/* Construct the dynamic length HWMON PWM config Attributes with null termination using GNU C Array
   Initializer construct */
#if defined(NUM_UCEXT_DENVERTON_PWMS) && (NUM_UCEXT_DENVERTON_PWMS>0)
const u32 denverton_pwm_config[NUM_UCEXT_DENVERTON_PWMS+1] = { [0 ... (NUM_UCEXT_DENVERTON_PWMS-1)] = (HWMON_PWM_INPUT | HWMON_PWM_ENABLE), [NUM_UCEXT_DENVERTON_PWMS] = 0}; 

/* Declare the Standard HWMON PWM Channel Info Structure */
const struct hwmon_channel_info denverton_pwm = {
	.type = hwmon_pwm,
	.config = denverton_pwm_config,
};
#endif

/*------------------------------------------------------------------------------------------------------------------*/
/* Construct the dynamic length HWMON Voltage config Attributes with null termination using GNU C Array
   Initializer construct */
#if defined(NUM_UCEXT_DENVERTON_VOLTS) && (NUM_UCEXT_DENVERTON_VOLTS>0)
const u32 denverton_volt_config[NUM_UCEXT_DENVERTON_VOLTS+1] = { [0 ... (NUM_UCEXT_DENVERTON_VOLTS-1)] = 
    (HWMON_I_INPUT|HWMON_I_LABEL|HWMON_I_MAX|HWMON_I_MAX_ALARM|HWMON_I_MIN|HWMON_I_LCRIT|HWMON_I_MIN_ALARM|HWMON_I_LCRIT_ALARM), [NUM_UCEXT_DENVERTON_VOLTS] = 0}; 

/* Declare the Standard HWMON Voltage Channel Info Structure */
const struct hwmon_channel_info denverton_volt = {
	.type = hwmon_in,
	.config = denverton_volt_config,
};
#endif

/*------------------------------------------------------------------------------------------------------------------*/
/* Construct the Standrad HWMON chip config attributes */
static const u32 denverton_chip_config[] = {
	HWMON_C_UPDATE_INTERVAL,
	0
};

/* Declare the Standard HWMON Chip Info Structure */
static const struct hwmon_channel_info denverton_chip = {
	.type = hwmon_chip,
	.config = denverton_chip_config,
};


/*------------------------------------------------------------------------------------------------------------------*/
/* HWMON Standard Chip Attribute Ops  */

static int denverton_read_chip(struct device *dev, u32 attr, int channel,
                             long *val)
{
    struct denverton_data *p_denverton_data = ucext_get_dev_denverton_data(dev);

    if( (p_denverton_data==NULL) || (channel<0) || (channel>=1) ) {
        return -EOPNOTSUPP;
    }

    if(p_denverton_data->valid==0) {
        dev_err(dev, "denverton_read_chip private denverton data not valid\n");
        return -ENODEV;
    }

    switch(attr) {
        case hwmon_chip_update_interval:
            mutex_lock(&p_denverton_data->update_lock);
            *val = p_denverton_data->chip_update_interval;
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        default:
            return -EOPNOTSUPP;
    }

    return 0;
}


static int denverton_write_chip(struct device *dev, u32 attr, int channel,
                              long val)
{
    struct denverton_data *p_denverton_data;

    if(dev==NULL) {
        return -ENODEV;
    }

    p_denverton_data = ucext_get_dev_denverton_data(dev);

    if(p_denverton_data==NULL) {
        dev_err(dev, "denverton_write_chip private NULL denverton private data ptr\n");
        return -ENODEV;
    }

    if(p_denverton_data->valid==0) {
        dev_err(dev, "denverton_write_chip private denverton data not valid\n");
        return -ENODEV;
    }

    if( (p_denverton_data==NULL) || (channel<0) || (channel>=1) ) {
        return -EOPNOTSUPP;
    }

    switch(attr) {
        case hwmon_chip_update_interval:
            if(val < UCEXT_DENVERTON_MIN_UPDATE_INTERVAL || val >UCEXT_DENVERTON_MAX_UPDATE_INTERVAL) {
                return -EINVAL;
            }
            mutex_lock(&p_denverton_data->update_lock);
            p_denverton_data->chip_update_interval=val;
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        default:
            return -EOPNOTSUPP;
    }

    return 0;
}


static umode_t denverton_chip_is_visible(const void *_data, u32 attr, int channel)
{
    if( (channel<0)||(channel>=1) ) {
        return 0;
    }

    switch(attr) {
        case hwmon_chip_update_interval:
            return S_IRUGO | S_IWUSR;
        default:
            break;
    }
    return 0;
}


/*------------------------------------------------------------------------------------------------------------------*/
/* HWMON Standard Temperature Attribute Ops  */

static int denverton_read_temp(struct device *dev, u32 attr, int channel,
                             long *val)
{
    struct denverton_data *p_denverton_data = ucext_get_dev_denverton_data(dev);
    int retval = 0;

    if( (p_denverton_data==NULL) || (channel<0) || (channel>=NUM_UCEXT_DENVERTON_TEMPS) ) {
        return -EOPNOTSUPP;
    }

    if(p_denverton_data->valid==0) {
        dev_err(dev, "denverton_read_temp private denverton data not valid\n");
        return -ENODEV;
    }

    retval = ucext_dev_denverton_update(dev, p_denverton_data);
    if(retval) {
        dev_err(dev, "denverton_read_temp call to ucext_dev_denverton_update failed\n");
        return -ENODEV;
    }

    switch(attr) {
        case hwmon_temp_input:
            mutex_lock(&p_denverton_data->update_lock);
            *val = p_denverton_data->temp_in_vals[channel];
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        case hwmon_temp_max:
            mutex_lock(&p_denverton_data->update_lock);
            *val = p_denverton_data->temp_maxs[channel];
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        case hwmon_temp_max_hyst:
            mutex_lock(&p_denverton_data->update_lock);
            *val = p_denverton_data->temp_max_hysts[channel];
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        case hwmon_temp_max_alarm:
            mutex_lock(&p_denverton_data->update_lock);
            *val = p_denverton_data->temp_max_alarms[channel];
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        case hwmon_temp_min:
            mutex_lock(&p_denverton_data->update_lock);
            *val = p_denverton_data->temp_mins[channel];
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        case hwmon_temp_min_hyst:
            mutex_lock(&p_denverton_data->update_lock);
            *val = p_denverton_data->temp_min_hysts[channel];
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        case hwmon_temp_min_alarm:
            mutex_lock(&p_denverton_data->update_lock);
            *val = p_denverton_data->temp_min_alarms[channel];
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        case hwmon_temp_crit:
            mutex_lock(&p_denverton_data->update_lock);
            *val = p_denverton_data->temp_crits[channel];
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        case hwmon_temp_crit_hyst:
            mutex_lock(&p_denverton_data->update_lock);
            *val = p_denverton_data->temp_crit_hysts[channel];
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        case hwmon_temp_crit_alarm:
            mutex_lock(&p_denverton_data->update_lock);
            *val = p_denverton_data->temp_crit_alarms[channel];
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        case hwmon_temp_emergency:
            mutex_lock(&p_denverton_data->update_lock);
            *val = p_denverton_data->temp_emergs[channel];
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        case hwmon_temp_emergency_hyst:
            mutex_lock(&p_denverton_data->update_lock);
            *val = p_denverton_data->temp_emerg_hysts[channel];
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        case hwmon_temp_emergency_alarm:
            mutex_lock(&p_denverton_data->update_lock);
            *val = p_denverton_data->temp_emerg_alarms[channel];
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        default:
            return -EOPNOTSUPP;
    }

    return retval;
}


static int denverton_write_temp(struct device *dev, u32 attr, int channel,
                              long val)
{
    struct denverton_data *p_denverton_data = ucext_get_dev_denverton_data(dev);
    int retval = 0;

    if( (p_denverton_data==NULL) || (channel<0) || (channel>=NUM_UCEXT_DENVERTON_TEMPS) ) {
        return -EOPNOTSUPP;
    }

    if(p_denverton_data->valid==0) {
        dev_err(dev, "denverton_write_temp private denverton data not valid\n");
        return -ENODEV;
    }

    switch(attr) {
        case hwmon_temp_min:
            mutex_lock(&p_denverton_data->update_lock);
            p_denverton_data->temp_mins[channel] = val;
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        case hwmon_temp_min_hyst:
            mutex_lock(&p_denverton_data->update_lock);
            p_denverton_data->temp_min_hysts[channel] = val;
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        case hwmon_temp_max:
            mutex_lock(&p_denverton_data->update_lock);
            p_denverton_data->temp_maxs[channel] = val;
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        case hwmon_temp_max_hyst:
            mutex_lock(&p_denverton_data->update_lock);
            p_denverton_data->temp_max_hysts[channel] = val;
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        case hwmon_temp_crit:
            mutex_lock(&p_denverton_data->update_lock);
            p_denverton_data->temp_crits[channel] = val;
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        case hwmon_temp_crit_hyst:
            mutex_lock(&p_denverton_data->update_lock);
            p_denverton_data->temp_crit_hysts[channel] = val;
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        case hwmon_temp_emergency:
            mutex_lock(&p_denverton_data->update_lock);
            p_denverton_data->temp_emergs[channel] = val;
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        case hwmon_temp_emergency_hyst:
            mutex_lock(&p_denverton_data->update_lock);
            p_denverton_data->temp_emerg_hysts[channel] = val;
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        default:
            return -EOPNOTSUPP;
    }

    retval = ucext_dev_denverton_update(dev, p_denverton_data);
    if(retval) {
        dev_err(dev, "denverton_write_temp call to ucext_dev_denverton_update failed\n");
        return -ENODEV;
    }

    return retval;
}


static umode_t denverton_temp_is_visible(const void *_data, u32 attr, int channel)
{
    if( (channel<0)||(channel>=NUM_UCEXT_DENVERTON_TEMPS) ) {
        return 0;
    }

    switch(attr) {
        case hwmon_temp_input:
        case hwmon_temp_emergency_alarm:
        case hwmon_temp_crit_alarm:
        case hwmon_temp_max_alarm:
        case hwmon_temp_min_alarm:
            return S_IRUGO;

        case hwmon_temp_emergency:
        case hwmon_temp_emergency_hyst:
        case hwmon_temp_crit:
        case hwmon_temp_crit_hyst:
        case hwmon_temp_max:
        case hwmon_temp_max_hyst:
        case hwmon_temp_min:
        case hwmon_temp_min_hyst:
            return S_IRUGO | S_IWUSR;
        default:
            break;
    }

    return 0;
}


/*------------------------------------------------------------------------------------------------------------------*/
/* HWMON Standard Current Attribute Ops  */

static int denverton_read_curr(struct device *dev, u32 attr, int channel,
                             long *val)
{
    struct denverton_data *p_denverton_data = ucext_get_dev_denverton_data(dev);
    int retval = 0;

    if( (p_denverton_data==NULL) || (channel<0) || (channel>=NUM_UCEXT_DENVERTON_CURRS) ) {
        return -EOPNOTSUPP;
    }

    if(p_denverton_data->valid==0) {
        dev_err(dev, "denverton_read_curr private denverton data not valid\n");
        return -ENODEV;
    }

    retval = ucext_dev_denverton_update(dev, p_denverton_data);
    if(retval) {
        dev_err(dev, "denverton_read_curr call to ucext_dev_denverton_update failed\n");
        return -ENODEV;
    }

    switch(attr) {
        case hwmon_curr_input:
            mutex_lock(&p_denverton_data->update_lock);
            *val = p_denverton_data->curr_in_vals[channel];
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        default:
            return -EOPNOTSUPP;
    }

    return retval;
}

static umode_t denverton_curr_is_visible(const void *_data, u32 attr, int channel)
{
    if( (channel<0)||(channel>=NUM_UCEXT_DENVERTON_CURRS) ) {
        return 0;
    }

    if(attr==hwmon_curr_input) {
        return S_IRUGO;
    }

    return 0;
}


/*------------------------------------------------------------------------------------------------------------------*/
/* HWMON Standard Fan Attribute Ops  */

static int denverton_read_fan(struct device *dev, u32 attr, int channel,
                            long *val)
{
    struct denverton_data *p_denverton_data = ucext_get_dev_denverton_data(dev);
    int retval = 0;

    if( (p_denverton_data==NULL) || (channel<0) || (channel>=NUM_UCEXT_DENVERTON_FANS) ) {
        return -EOPNOTSUPP;
    }

    if(p_denverton_data->valid==0) {
        dev_err(dev, "denverton_read_fan private denverton data not valid\n");
        return -ENODEV;
    }

    retval = ucext_dev_denverton_update(dev, p_denverton_data);
    if(retval) {
        dev_err(dev, "denverton_read_fan call to ucext_dev_denverton_update failed\n");
        return -ENODEV;
    }

    switch(attr) {
        case hwmon_fan_input:
            mutex_lock(&p_denverton_data->update_lock);
            *val = p_denverton_data->fan_in_vals[channel];
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        case hwmon_fan_alarm:
            mutex_lock(&p_denverton_data->update_lock);
            *val = p_denverton_data->fan_alarms[channel];
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        case hwmon_fan_fault:
            mutex_lock(&p_denverton_data->update_lock);
            *val = p_denverton_data->fan_faults[channel];
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        default:
            return -EOPNOTSUPP;
    }

    return retval;
}


static umode_t denverton_fan_is_visible(const void *_data, u32 attr, int channel)
{
    if( (channel<0)||(channel>=NUM_UCEXT_DENVERTON_FANS) ) {
        return 0;
    }

    if(attr==hwmon_fan_input) {
        return S_IRUGO;
    }
    if(attr==hwmon_fan_alarm) {
        return S_IRUGO;
    }
    if(attr==hwmon_fan_fault) {
        return S_IRUGO;
    }

    return 0;
}


/*------------------------------------------------------------------------------------------------------------------*/
/* HWMON Standard PWM Attribute Ops  */

static int denverton_read_pwm(struct device *dev, u32 attr, int channel,
                            long *val)
{
    struct denverton_data *p_denverton_data = ucext_get_dev_denverton_data(dev);
    int retval = 0;

    if( (p_denverton_data==NULL) || (channel<0) || (channel>=NUM_UCEXT_DENVERTON_PWMS) ) {
        return -EOPNOTSUPP;
    }

    if(p_denverton_data->valid==0) {
        dev_err(dev, "denverton_read_pwm private denverton data not valid\n");
        return -ENODEV;
    }

    retval = ucext_dev_denverton_update(dev, p_denverton_data);
    if(retval) {
        dev_err(dev, "denverton_read_pwm call to ucext_dev_denverton_update failed\n");
        return -ENODEV;
    }

    switch(attr) {
        case hwmon_pwm_input:
            mutex_lock(&p_denverton_data->update_lock);
            *val = p_denverton_data->pwm_in_vals[channel];
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        case hwmon_pwm_enable:
            mutex_lock(&p_denverton_data->update_lock);
            *val = p_denverton_data->pwm_en_vals[channel];
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        default:
            return -EOPNOTSUPP;
    }

    return retval;
}

static int denverton_write_pwm(struct device *dev, u32 attr, int channel,
                             long val)
{
    struct denverton_data *p_denverton_data;
    void *p_msg_data;
    int retval = 0;

    if(dev==NULL) {
        return -ENODEV;
    }

    p_denverton_data = ucext_get_dev_denverton_data(dev);

    if(p_denverton_data==NULL) {
        dev_err(dev, "denverton_write_pwm private NULL denverton private data ptr\n");
        return -ENODEV;
    }

    if(p_denverton_data->valid==0) {
        dev_err(dev, "denverton_write_pwm private denverton data not valid\n");
        return -ENODEV;
    }

    p_msg_data = ucext_get_dev_msg_data(dev);
    if(!p_msg_data) {
        dev_err(dev, "denverton_write_pwm null pointer to private message data\n");
        return -ENODEV;
    }

    if( (p_denverton_data==NULL) || (p_msg_data==NULL) || (channel<0) || (channel>=NUM_UCEXT_DENVERTON_FANS) ) {
        return -EOPNOTSUPP;
    }

    switch(attr) {
        case hwmon_pwm_input:
            if(val < 0 || val > 255) {
                return -EINVAL;
            }
            mutex_lock(&p_denverton_data->update_lock);
            p_denverton_data->pwm_in_vals[channel]=val;
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        case hwmon_pwm_enable:
            if(val < 1 || val > 2) {
                return -EINVAL;
            }
            mutex_lock(&p_denverton_data->update_lock);
            p_denverton_data->pwm_en_vals[channel]=val;
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        default:
            return -EOPNOTSUPP;
    }

    retval = ucext_msg_set_pwms(UCEXT_DENVERTON_DEVICE_INDEX, p_msg_data, p_denverton_data->pwm_en_vals, p_denverton_data->pwm_in_vals );

    if(retval) {
        dev_err(dev, "denverton_write_pwm call to ucext_msg_set_pwms failed\n");
        return -ENODEV;
    }

    return retval;
}


static umode_t denverton_pwm_is_visible(const void *_data, u32 attr, int channel)
{
    if( (channel<0)||(channel>=NUM_UCEXT_DENVERTON_PWMS) ) {
        return 0;
    }

    switch(attr) {
        case hwmon_pwm_input:
        case hwmon_pwm_enable:
            return S_IRUGO | S_IWUSR;
        default:
            break;
    }
    return 0;
}

/*------------------------------------------------------------------------------------------------------------------*/
/* HWMON Standard Voltage Attribute Ops  */

static int denverton_read_volt(struct device *dev, u32 attr, int channel,
                             long *val)
{
    struct denverton_data *p_denverton_data = ucext_get_dev_denverton_data(dev);
    int retval = 0;

    if( (p_denverton_data==NULL) || (channel<0) || (channel>=NUM_UCEXT_DENVERTON_VOLTS) ) {
        return -EOPNOTSUPP;
    }

    if(p_denverton_data->valid==0) {
        dev_err(dev, "denverton_read_volt private denverton data not valid\n");
        return -ENODEV;
    }

    retval = ucext_dev_denverton_update(dev, p_denverton_data);
    if(retval) {
        dev_err(dev, "denverton_read_volt call to ucext_dev_denverton_update failed\n");
        return -ENODEV;
    }

    switch(attr) {
        case hwmon_in_input:
            mutex_lock(&p_denverton_data->update_lock);
            *val = p_denverton_data->volt_in_vals[channel];
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        case hwmon_in_max:
            mutex_lock(&p_denverton_data->update_lock);
            *val = p_denverton_data->volt_maxs[channel];
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        case hwmon_in_min:
            mutex_lock(&p_denverton_data->update_lock);
            *val = p_denverton_data->volt_mins[channel];
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        case hwmon_in_lcrit:
            mutex_lock(&p_denverton_data->update_lock);
            *val = p_denverton_data->volt_lcrits[channel];
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        case hwmon_in_max_alarm:
            mutex_lock(&p_denverton_data->update_lock);
            *val = p_denverton_data->volt_max_alarms[channel];
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        case hwmon_in_min_alarm:
            mutex_lock(&p_denverton_data->update_lock);
            *val = p_denverton_data->volt_min_alarms[channel];
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        case hwmon_in_lcrit_alarm:
            mutex_lock(&p_denverton_data->update_lock);
            *val = p_denverton_data->volt_lcrit_alarms[channel];
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        default:
            return -EOPNOTSUPP;
    }

    return retval;
}


static int denverton_write_volt(struct device *dev, u32 attr, int channel,
                            long val)
{
    struct denverton_data *p_denverton_data = ucext_get_dev_denverton_data(dev);
    int retval = 0;

    if( (p_denverton_data==NULL) || (channel<0) || (channel>=NUM_UCEXT_DENVERTON_VOLTS) ) {
        return -EOPNOTSUPP;
    }

    if(p_denverton_data->valid==0) {
        dev_err(dev, "denverton_write_volt private denverton data not valid\n");
        return -ENODEV;
    }

    switch(attr) {
        case hwmon_in_max:
            mutex_lock(&p_denverton_data->update_lock);
            p_denverton_data->volt_maxs[channel] = val;
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        case hwmon_in_min:
            mutex_lock(&p_denverton_data->update_lock);
            p_denverton_data->volt_mins[channel] = val;
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        case hwmon_in_lcrit:
            mutex_lock(&p_denverton_data->update_lock);
            p_denverton_data->volt_lcrits[channel] = val;
            mutex_unlock(&p_denverton_data->update_lock);
            break;
        default:
            return -EOPNOTSUPP;
    }

    retval = ucext_dev_denverton_update(dev, p_denverton_data);
    if(retval) {
        dev_err(dev, "denverton_write_volt call to ucext_dev_denverton_update failed\n");
        return -ENODEV;
    }

    return retval;
}


static umode_t denverton_volt_is_visible(const void *_data, u32 attr, int channel)
{
    if( (channel<0)||(channel>=NUM_UCEXT_DENVERTON_VOLTS) ) {
        return 0;
    }

    if(attr==hwmon_in_input) {
        return S_IRUGO;
    }

    if(attr==hwmon_in_max_alarm) {
        return S_IRUGO;
    }

    if(attr==hwmon_in_min_alarm) {
        return S_IRUGO;
    }

    if(attr==hwmon_in_lcrit_alarm) {
        return S_IRUGO;
    }

    if(attr==hwmon_in_max) {
        return S_IRUGO | S_IWUSR;
    }

    if(attr==hwmon_in_min) {
        return S_IRUGO | S_IWUSR;
    }

    if(attr==hwmon_in_lcrit) {
        return S_IRUGO | S_IWUSR;
    }

    return 0;
}

/*------------------------------------------------------------------------------------------------------------------*/
/* DENVERTON Multi-Purpose Device subsystem HWMON register by info Ops and structs                                      */
/*------------------------------------------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------------------------------------------*/
/* HWMON standard info attribute operations accessor functions   */

static int denverton_read(struct device *dev, enum hwmon_sensor_types type,
                        u32 attr, int channel, long *val)
{
    switch(type) {
        case hwmon_chip:
            return denverton_read_chip(dev, attr, channel, val);
        case hwmon_curr:
            return denverton_read_curr(dev, attr, channel, val);
        case hwmon_fan:
            return denverton_read_fan(dev, attr, channel, val);
        case hwmon_pwm:
            return denverton_read_pwm(dev, attr, channel, val);
        case hwmon_temp:
            return denverton_read_temp(dev, attr, channel, val);
        case hwmon_in:
            return denverton_read_volt(dev, attr, channel, val);
        default:
            return -EOPNOTSUPP;
    }
}

static int denverton_write(struct device *dev, enum hwmon_sensor_types type,
                         u32 attr, int channel, long val)
{
    switch(type) {
        case hwmon_chip:
            return denverton_write_chip(dev, attr, channel, val);
        case hwmon_pwm:
            return denverton_write_pwm(dev, attr, channel, val);
        case hwmon_temp:
            return denverton_write_temp(dev, attr, channel, val);
        case hwmon_in:
            return denverton_write_volt(dev, attr, channel, val);
        default:
            return -EOPNOTSUPP;
    }
}

static umode_t denverton_is_visible(const void *data,
                                  enum hwmon_sensor_types type,
                                  u32 attr, int channel)
{
    switch(type) {
        case hwmon_chip:
            return denverton_chip_is_visible(data, attr, channel);
        case hwmon_curr:
            return denverton_curr_is_visible(data, attr, channel);
        case hwmon_fan:
            return denverton_fan_is_visible(data, attr, channel);
        case hwmon_pwm:
            return denverton_pwm_is_visible(data, attr, channel);
        case hwmon_temp:
            return denverton_temp_is_visible(data, attr, channel);
        case hwmon_in:
            return denverton_volt_is_visible(data, attr, channel);
        default:
            return 0;
    }
}

/*------------------------------------------------------------------------------------------------------------------*/
/* HWMON standard info structs for registration   */

static const struct hwmon_channel_info *denverton_info[] = {
    &denverton_chip,
#if defined(NUM_UCEXT_DENVERTON_CURRS) && (NUM_UCEXT_DENVERTON_CURRS>0)
    &denverton_curr,
#endif
#if defined(NUM_UCEXT_DENVERTON_FANS) && (NUM_UCEXT_DENVERTON_FANS>0)
    &denverton_fan,
#endif
#if defined(NUM_UCEXT_DENVERTON_PWMS) && (NUM_UCEXT_DENVERTON_PWMS>0)
    &denverton_pwm,
#endif
#if defined(NUM_UCEXT_DENVERTON_TEMPS) && (NUM_UCEXT_DENVERTON_TEMPS>0)
    &denverton_temp,
#endif
#if defined(NUM_UCEXT_DENVERTON_VOLTS) && (NUM_UCEXT_DENVERTON_VOLTS>0)
    &denverton_volt,
#endif
    NULL
};

static const struct hwmon_ops denverton_hwmon_ops = {
    .is_visible = denverton_is_visible,
    .read = denverton_read,
    .write = denverton_write,
};


static const struct hwmon_chip_info ucext_chip_info = {
    .ops = &denverton_hwmon_ops,
    .info = denverton_info,
};

/*------------------------------------------------------------------------------------------------------------------*/
/* Non-Standard HWMON Attributes to be registered by "groups"                                                       */
/*------------------------------------------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------------------------------------------*/
/* HWMON Non-standard attribute operations    */

#if defined(NUM_UCEXT_DENVERTON_LEDS) && (NUM_UCEXT_DENVERTON_LEDS>0)

static ssize_t denverton_ns_show_led_color(struct device *dev,
                                         struct device_attribute *dev_attr, char *buf)
{
    struct denverton_data *p_denverton_data = ucext_get_dev_denverton_data(dev);
    struct sensor_device_attribute *attr = to_sensor_dev_attr(dev_attr);
    int channel=0;
    int res = 0;

    if(attr==NULL) {
        return -EOPNOTSUPP;
    }
    channel = attr->index;

    if( (p_denverton_data==NULL) || (channel<0) || (channel>=NUM_UCEXT_DENVERTON_LEDS) ) {
        return -EOPNOTSUPP;
    }

    mutex_lock(&p_denverton_data->update_lock);
    res = sprintf(buf, "%d\n", p_denverton_data->led_color_vals[channel]);
    mutex_unlock(&p_denverton_data->update_lock);

    //dev_err(dev, "denverton_ns_show_led_color LED[%d] was %d\n", channel, p_denverton_data->led_color_vals[channel]);

    return res;
}

static ssize_t denverton_ns_show_led_rate(struct device *dev,
                                        struct device_attribute *dev_attr, char *buf)
{
    struct denverton_data *p_denverton_data = ucext_get_dev_denverton_data(dev);
    struct sensor_device_attribute *attr = to_sensor_dev_attr(dev_attr);
    int channel=0;
    int res = 0;

    if(attr==NULL) {
        return -EOPNOTSUPP;
    }
    channel = attr->index;

    if( (p_denverton_data==NULL) || (channel<0) || (channel>=NUM_UCEXT_DENVERTON_LEDS) ) {
        return -EOPNOTSUPP;
    }

    mutex_lock(&p_denverton_data->update_lock);
    res = sprintf(buf, "%d\n", p_denverton_data->led_rate_vals[channel]);
    mutex_unlock(&p_denverton_data->update_lock);

    //dev_err(dev, "denverton_ns_show_led_rate LED[%d] was %d\n", channel, p_denverton_data->led_rate_vals[channel]);

    return res;
}

static ssize_t denverton_ns_store_led_color(struct device *dev,
                                          struct device_attribute *dev_attr,
                                          const char *buf, size_t count)
{
    struct denverton_data *p_denverton_data = ucext_get_dev_denverton_data(dev);
    void *p_msg_data = ucext_get_dev_msg_data(dev);
    struct sensor_device_attribute *attr = to_sensor_dev_attr(dev_attr);
    int channel = 0;
    int val = 0;
    int res = 0;

    if( (p_denverton_data==NULL) || (p_msg_data==NULL) || (attr==NULL) ) {
        dev_err(dev, "denverton_ns_store_led_color NULL pointer to private data failure\n");
        return -EOPNOTSUPP;
    }

    channel = attr->index;

    if( (channel<0) || (channel>=NUM_UCEXT_DENVERTON_LEDS) ) {
        return -EOPNOTSUPP;
    }

    res = kstrtoint(buf, 10, &val);
    if(res)
        return res;

    val = clamp_val(val, 0, 3);

    //dev_err(dev, "denverton_ns_store_led_color LED[%d]=%d\n", channel, val);

    mutex_lock(&p_denverton_data->update_lock);
    p_denverton_data->led_color_vals[channel] = val;
    res = ucext_msg_set_leds(UCEXT_DENVERTON_DEVICE_INDEX, p_msg_data, p_denverton_data->led_color_vals, p_denverton_data->led_rate_vals );
    mutex_unlock(&p_denverton_data->update_lock);

    if(res) {
        dev_err(dev, "denverton_ns_store_led_color call to ucext_msg_set_leds failed\n");
        return -ENODEV;
    }

    return count;
}

static ssize_t denverton_ns_store_led_rate(struct device *dev,
                                         struct device_attribute *dev_attr,
                                         const char *buf, size_t count)
{
    struct denverton_data *p_denverton_data = ucext_get_dev_denverton_data(dev);
    void *p_msg_data = ucext_get_dev_msg_data(dev);
    struct sensor_device_attribute *attr = to_sensor_dev_attr(dev_attr);
    int channel = 0;
    int val = 0;
    int res = 0;

    if( (p_denverton_data==NULL) || (p_msg_data==NULL) || (attr==NULL) ) {
        dev_err(dev, "denverton_ns_store_led_rate NULL pointer to private data failure\n");
        return -EOPNOTSUPP;
    }

    channel = attr->index;

    if( (channel<0) || (channel>=NUM_UCEXT_DENVERTON_LEDS) ) {
        return -EOPNOTSUPP;
    }

    res = kstrtoint(buf, 10, &val);
    if(res)
        return res;

    val = clamp_val(val, 0, 3);

    //dev_err(dev, "denverton_ns_store_led_rate LED[%d]=%d\n", channel, val);

    mutex_lock(&p_denverton_data->update_lock);
    p_denverton_data->led_rate_vals[channel] = val;
    res = ucext_msg_set_leds(UCEXT_DENVERTON_DEVICE_INDEX, p_msg_data, p_denverton_data->led_color_vals, p_denverton_data->led_rate_vals );
    mutex_unlock(&p_denverton_data->update_lock);

    if(res) {
        dev_err(dev, "denverton_ns_store_led_rate call to ucext_msg_set_leds failed\n");
        return -ENODEV;
    }

    return count;
}


static umode_t denverton_ns_rw_led_attrs_visible(struct kobject *kobj,
                                               struct attribute *attr, int n)
{
    if(n>=NUM_UCEXT_DENVERTON_LEDS) {
        return 0;
    }
    return(S_IWUSR|S_IRUGO);
}

#endif

static ssize_t denverton_ns_show_chip(struct device *dev,
                                    struct device_attribute *dev_attr, char *buf)
{
    struct denverton_data *p_denverton_data = ucext_get_dev_denverton_data(dev);
    struct sensor_device_attribute *attr = to_sensor_dev_attr(dev_attr);
    int res = 0;

    if(attr==NULL) {
        return -EOPNOTSUPP;
    }

    if( (p_denverton_data==NULL) ) {
        return -EOPNOTSUPP;
    }

    // call for the alarm/fault updcates
    res = ucext_dev_denverton_update(dev, p_denverton_data);
    if(res) {
        dev_err(dev, "denverton_ns_show_chip call to ucext_dev_denverton_update failed\n");
        return -ENODEV;
    }

    mutex_lock(&p_denverton_data->update_lock);

    /* For non-standard attributes, have to match the 'name' to determine which one  being accessed */
    if(strcmp("fw_major",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "%d\n", p_denverton_data->chip_fw_major);
    }
    else if(strcmp("fw_minor",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "%d\n", p_denverton_data->chip_fw_minor);
    }
    else if(strcmp("msg_protocol",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "%d\n", p_denverton_data->chip_protocol);
    }
    else if(strcmp("hw_id",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "%d\n", p_denverton_data->chip_hw_id);
    }
#if defined(UCEXT_DENVERTON_DEVICE_INDEX) && (UCEXT_DENVERTON_DEVICE_INDEX==0)
    else if(strcmp("boot_major",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "%d\n", p_denverton_data->chip_boot_major);
    }
    else if(strcmp("boot_minor",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "%d\n", p_denverton_data->chip_boot_minor);
    }
    else if(strcmp("msg_debug",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "%d\n", ucext_get_dev_msg_debug(dev) );
    }
    else if(strcmp("clk1_major",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "%d\n", p_denverton_data->chip_clk1_major);
    }
    else if(strcmp("clk1_minor",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "%d\n", p_denverton_data->chip_clk1_minor);
    }
    else if(strcmp("reset_cause",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "%d\n", p_denverton_data->chip_reset_cause );
    }
    else if(strcmp("watchdog",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "%d\n", p_denverton_data->chip_watchdog );
    }
#endif
    else if(strcmp("dev_status",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "0x%04x\n", p_denverton_data->chip_dev_status );
    }
#if defined(UCEXT_DENVERTON_DEVICE_INDEX) && (UCEXT_DENVERTON_DEVICE_INDEX==0)
    else if(strcmp("aux_dev1",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "0x%02x\n", p_denverton_data->chip_aux_dev1 );
    }
    else if(strcmp("aux_dev2",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "0x%02x\n", p_denverton_data->chip_aux_dev2 );
    }
    else if(strcmp("aux_dev3",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "0x%02x\n", p_denverton_data->chip_aux_dev3 );
    }
    else if(strcmp("aux_dev4",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "0x%02x\n", p_denverton_data->chip_aux_dev4 );
    }
    else if(strcmp("aux_dev5",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "0x%02x\n", p_denverton_data->chip_aux_dev5 );
    }
    else if(strcmp("aux_dev6",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "0x%02x\n", p_denverton_data->chip_aux_dev6 );
    }
    else if(strcmp("aux_clk1",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "0x%02x\n", p_denverton_data->chip_aux_clk1 );
    }
    else if(strcmp("aux_prog1",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "0x%02x\n", p_denverton_data->chip_aux_prog1 );
    }
    else if(strcmp("aux_therm1",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "0x%02x\n", p_denverton_data->chip_aux_therm1 );
    }
    else if(strcmp("aux_therm2",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "0x%02x\n", p_denverton_data->chip_aux_therm2 );
    }
    else if(strcmp("aux_therm3",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "0x%02x\n", p_denverton_data->chip_aux_therm3 );
    }
    else if(strcmp("aux_therm4",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "0x%02x\n", p_denverton_data->chip_aux_therm4 );
    }
    else if(strcmp("aux_therm5",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "0x%02x\n", p_denverton_data->chip_aux_therm5 );
    }
    else if(strcmp("aux_therm6",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "0x%02x\n", p_denverton_data->chip_aux_therm6 );
    }
    else if(strcmp("aux_therm7",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "0x%02x\n", p_denverton_data->chip_aux_therm7 );
    }
    else if(strcmp("aux_therm8",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "0x%02x\n", p_denverton_data->chip_aux_therm8 );
    }
    else if(strcmp("aux_therm9",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "0x%02x\n", p_denverton_data->chip_aux_therm9 );
    }
    else if(strcmp("aux_therm10",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "0x%02x\n", p_denverton_data->chip_aux_therm10 );
    }
    else if(strcmp("dev_rxpkts",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "%u\n", p_denverton_data->chip_dev_rxpkts );
    }
    else if(strcmp("dev_txpkts",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "%u\n", p_denverton_data->chip_dev_txpkts );
    }
#endif
#if defined(UCEXT_DENVERTON_SYSTEM_FAN) && (UCEXT_DENVERTON_SYSTEM_FAN!=0)
    else if(strcmp("fan_alarm",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "%d\n", p_denverton_data->system_fan_alarm);
    }
    else if(strcmp("fan_fault",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "%d\n", p_denverton_data->system_fan_fault);
    }
    else if(strcmp("fan_input",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "%d\n", p_denverton_data->system_fan_avg);
    }
#endif
#if defined(UCEXT_DENVERTON_VIN_ALARMS) && (UCEXT_DENVERTON_VIN_ALARMS>0)
    else if(strcmp("vin1_alarm",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "%d\n", ( ((p_denverton_data->pwr_good_bits&(~p_denverton_data->pwr_test_bits))&0x4000) ? 0 : 1)  );
    }
    else if(strcmp("vin2_alarm",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "%d\n", ( ((p_denverton_data->pwr_good_bits&(~p_denverton_data->pwr_test_bits))&0x8000) ? 0 : 1)  );
    }
#endif
    else {
        res = -EOPNOTSUPP;
        goto error;
    }

    error:
    mutex_unlock(&p_denverton_data->update_lock);
    return res;
}


#if defined(UCEXT_DENVERTON_DEVICE_INDEX) && (UCEXT_DENVERTON_DEVICE_INDEX==0)
static ssize_t denverton_ns_store_chip(struct device *dev,
                                     struct device_attribute *dev_attr,
                                     const char *buf, size_t count)
{
    struct denverton_data *p_denverton_data = ucext_get_dev_denverton_data(dev);
    void *p_msg_data = ucext_get_dev_msg_data(dev);
    struct sensor_device_attribute *attr = to_sensor_dev_attr(dev_attr);
    int val = 0;
    int res = 0;

    if( (p_denverton_data==NULL) || (p_msg_data==NULL) || (attr==NULL) ) {
        dev_err(dev, "denverton_ns_store_chip NULL pointer to private data failure\n");
        return -EOPNOTSUPP;
    }

    res = kstrtouint(buf, 10, &val);
    if(res)
        return res;

    /* For non-standard attributes, have to match the 'name' to determine which one  being accessed */
    if(strcmp("msg_debug",attr->dev_attr.attr.name)==0) {
        if(ucext_set_dev_msg_debug(dev, val)==0) {
            return count;
        }
    }
    else if(strcmp("reboot",attr->dev_attr.attr.name)==0) {
        if( (val>=0) && (val<=1) ) {
            mutex_lock(&p_denverton_data->update_lock);
            p_denverton_data->chip_reboot = val;
            res = ucext_msg_set_reboot(UCEXT_DENVERTON_DEVICE_INDEX, p_msg_data, &p_denverton_data->chip_reboot);
            mutex_unlock(&p_denverton_data->update_lock);
            if(res) {
                dev_err(dev, "denverton_ns_store_chip call to ucext_msg_set_reboot failed\n");
                return -ENODEV;
            }
            return count;
        }
    }
    else if(strcmp("watchdog",attr->dev_attr.attr.name)==0) {
        if( (val>=0) && (val<=255) ) {
            mutex_lock(&p_denverton_data->update_lock);
            p_denverton_data->chip_watchdog = val;
            res = ucext_msg_set_watchdog(UCEXT_DENVERTON_DEVICE_INDEX, p_msg_data, &p_denverton_data->chip_watchdog);
            mutex_unlock(&p_denverton_data->update_lock);
            if(res) {
                dev_err(dev, "denverton_ns_store_chip call to ucext_msg_set_watchdog failed\n");
                return -ENODEV;
            }
            return count;
        }
    }

    return -EOPNOTSUPP;
}
#endif


static umode_t denverton_ns_chip_attrs_visible(struct kobject *kobj,
                                              struct attribute *attr, int n)
{
#if (defined(NUM_UCEXT_DENVERTON_FANS)&&(NUM_UCEXT_DENVERTON_FANS>0)) || (defined(NUM_UCEXT_DENVERTON_VOLTS) && (NUM_UCEXT_DENVERTON_VOLTS>0))
    /* For non-standard attributes, have to match the 'name' to determine which one  being accessed */
    if(strcmp("fan_test_alarms",attr->name)==0) {
        return(S_IWUSR|S_IRUGO);
    }
    if(strcmp("fan_test_faults",attr->name)==0) {
        return(S_IWUSR|S_IRUGO);
    }
#endif
#if defined(NUM_UCEXT_DENVERTON_VOLTS) && (NUM_UCEXT_DENVERTON_VOLTS>0) && defined(UCEXT_DENVERTON_PWRGOOD) && (UCEXT_DENVERTON_PWRGOOD>0)
    if(strcmp("in_test_pwrgood",attr->name)==0) {
        return(S_IWUSR|S_IRUGO);
    }
    if(strcmp("in_pwrgood",attr->name)==0) {
        return(S_IRUGO);
    }
#endif
    if(strcmp("vin1_alarm",attr->name)==0) {
        return(S_IRUGO);
    }
    if(strcmp("vin2_alarm",attr->name)==0) {
        return(S_IRUGO);
    }
#if defined(UCEXT_DENVERTON_DEVICE_INDEX) && (UCEXT_DENVERTON_DEVICE_INDEX==0)
    if(strcmp("msg_debug",attr->name)==0) {
        return(S_IWUSR|S_IRUGO);
    }
    if(strcmp("reset_cause",attr->name)==0) {
        return(S_IRUGO);
    }
    if(strcmp("watchdog",attr->name)==0) {
        return(S_IWUSR|S_IRUGO);
    }
    if(strcmp("reboot",attr->name)==0) {
        return(S_IWUSR);
    }
#endif
    if(strcmp("dev_status",attr->name)==0) {
        return(S_IRUGO);
    }
#if defined(UCEXT_DENVERTON_DEVICE_INDEX) && (UCEXT_DENVERTON_DEVICE_INDEX==0)
    if(strcmp("aux_dev1",attr->name)==0) {
        return(S_IRUGO);
    }
    if(strcmp("aux_dev2",attr->name)==0) {
        return(S_IRUGO);
    }
    if(strcmp("aux_dev3",attr->name)==0) {
        return(S_IRUGO);
    }
    if(strcmp("aux_dev4",attr->name)==0) {
        return(S_IRUGO);
    }
    if(strcmp("aux_dev5",attr->name)==0) {
        return(S_IRUGO);
    }
    if(strcmp("aux_dev6",attr->name)==0) {
        return(S_IRUGO);
    }
    if(strcmp("aux_clk1",attr->name)==0) {
        return(S_IRUGO);
    }
    if(strcmp("aux_prog1",attr->name)==0) {
        return(S_IRUGO);
    }
    if(strcmp("aux_therm1",attr->name)==0) {
        return(S_IRUGO);
    }
    if(strcmp("aux_therm2",attr->name)==0) {
        return(S_IRUGO);
    }
    if(strcmp("aux_therm3",attr->name)==0) {
        return(S_IRUGO);
    }
    if(strcmp("aux_therm4",attr->name)==0) {
        return(S_IRUGO);
    }
    if(strcmp("aux_therm5",attr->name)==0) {
        return(S_IRUGO);
    }
    if(strcmp("aux_therm6",attr->name)==0) {
        return(S_IRUGO);
    }
    if(strcmp("aux_therm7",attr->name)==0) {
        return(S_IRUGO);
    }
    if(strcmp("aux_therm8",attr->name)==0) {
        return(S_IRUGO);
    }
    if(strcmp("aux_therm9",attr->name)==0) {
        return(S_IRUGO);
    }
    if(strcmp("aux_therm10",attr->name)==0) {
        return(S_IRUGO);
    }
    if(strcmp("dev_rxpkts",attr->name)==0) {
        return(S_IRUGO);
    }
    if(strcmp("dev_txpkts",attr->name)==0) {
        return(S_IRUGO);
    }
#endif
    return(S_IRUGO);
}


#if defined(NUM_UCEXT_DENVERTON_BUTTONS) && (NUM_UCEXT_DENVERTON_BUTTONS>0)

static ssize_t denverton_ns_show_button(struct device *dev,
                                      struct device_attribute *dev_attr, char *buf)
{
    struct denverton_data *p_denverton_data = ucext_get_dev_denverton_data(dev);
    struct sensor_device_attribute *attr = to_sensor_dev_attr(dev_attr);
    int channel=0;
    int res = 0;

    if(attr==NULL) {
        return -EOPNOTSUPP;
    }

    channel = attr->index;

    if( (p_denverton_data==NULL) || (channel<0) || (channel>=NUM_UCEXT_DENVERTON_BUTTONS) ) {
        return -EOPNOTSUPP;
    }

    if(p_denverton_data->valid==0) {
        dev_err(dev, "denverton_ns_show_button private denverton data not valid\n");
        return -ENODEV;
    }

    res = ucext_dev_denverton_update(dev, p_denverton_data);
    if(res) {
        dev_err(dev, "denverton_ns_show_button call to ucext_dev_denverton_update failed\n");
        return -ENODEV;
    }

    mutex_lock(&p_denverton_data->update_lock);
    res = sprintf(buf, "%d\n", p_denverton_data->button_vals[channel]);
    mutex_unlock(&p_denverton_data->update_lock);

    return res;
}


static ssize_t denverton_ns_store_button(struct device *dev,
                                       struct device_attribute *dev_attr,
                                       const char *buf, size_t count)
{
    struct denverton_data *p_denverton_data = ucext_get_dev_denverton_data(dev);
    void *p_msg_data = ucext_get_dev_msg_data(dev);
    struct sensor_device_attribute *attr = to_sensor_dev_attr(dev_attr);
    int channel = 0;
    int val = 0;
    int res = 0;

    if( (p_denverton_data==NULL) || (p_msg_data==NULL) || (attr==NULL) ) {
        dev_err(dev, "denverton_ns_store_button NULL pointer to private data failure\n");
        return -EOPNOTSUPP;
    }

    channel = attr->index;

    if( (channel<0) || (channel>=NUM_UCEXT_DENVERTON_BUTTONS) ) {
        return -EOPNOTSUPP;
    }

    res = kstrtoint(buf, 10, &val);
    if(res)
        return res;

    /* Only value written is 0 to clear entries*/
    val = 0;
    mutex_lock(&p_denverton_data->update_lock);
    p_denverton_data->button_vals[channel] = val;
    res = ucext_msg_clear_button(UCEXT_DENVERTON_DEVICE_INDEX, p_msg_data, channel );
    mutex_unlock(&p_denverton_data->update_lock);

    if(res) {
        dev_err(dev, "denverton_ns_store_button call to ucext_msg_clr_buttons failed\n");
        return -ENODEV;
    }

    return count;
}

static umode_t denverton_ns_rw_button_attrs_visible(struct kobject *kobj,
                                                  struct attribute *attr, int n)
{
    if(n>=NUM_UCEXT_DENVERTON_BUTTONS) {
        return 0;
    }
    return(S_IWUSR|S_IRUGO);
}

#endif

#if ( (defined(NUM_UCEXT_DENVERTON_FANS)&&(NUM_UCEXT_DENVERTON_FANS>0)) || \
      (defined(NUM_UCEXT_DENVERTON_VOLTS) && (NUM_UCEXT_DENVERTON_VOLTS>0) && defined(UCEXT_DENVERTON_PWRGOOD) && (UCEXT_DENVERTON_PWRGOOD>0)) )

static ssize_t denverton_ns_show_test_hex(struct device *dev,
                                    struct device_attribute *dev_attr, char *buf)
{
    struct denverton_data *p_denverton_data = ucext_get_dev_denverton_data(dev);
    struct sensor_device_attribute *attr = to_sensor_dev_attr(dev_attr);
    int res = 0;

    if(attr==NULL) {
        return -EOPNOTSUPP;
    }

    if( (p_denverton_data==NULL) ) {
        return -EOPNOTSUPP;
    }

    // call for the alarm/fault updcates
    res = ucext_dev_denverton_update(dev, p_denverton_data);
    if(res) {
        dev_err(dev, "denverton_ns_show_test_hex call to ucext_dev_denverton_update failed\n");
        return -ENODEV;
    }

    mutex_lock(&p_denverton_data->update_lock);

    /* For non-standard attributes, have to match the 'name' to determine which one  being accessed */
    if(strcmp("fan_test_alarms",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "0x%04x\n", p_denverton_data->fan_test_alarm_bits);
    }
    else if(strcmp("fan_test_faults",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "0x%04x\n", p_denverton_data->fan_test_fault_bits);
    }
    else if(strcmp("in_test_pwrgood",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "0x%04x\n", p_denverton_data->pwr_test_bits&0xffff);
    }
    else if(strcmp("in_pwrgood",attr->dev_attr.attr.name)==0) {
        res = sprintf(buf, "0x%04x\n", (p_denverton_data->pwr_good_bits&(~p_denverton_data->pwr_test_bits)&0xffff) );
    }
    else {
        res = -EOPNOTSUPP;
        goto error;
    }

    error:
    mutex_unlock(&p_denverton_data->update_lock);
    return res;
}


static ssize_t denverton_ns_store_test_hex(struct device *dev,
                                     struct device_attribute *dev_attr,
                                     const char *buf, size_t count)
{
    struct denverton_data *p_denverton_data = ucext_get_dev_denverton_data(dev);
    void *p_msg_data = ucext_get_dev_msg_data(dev);
    struct sensor_device_attribute *attr = to_sensor_dev_attr(dev_attr);
    int val = 0;
    int res = 0;

    if( (p_denverton_data==NULL) || (p_msg_data==NULL) || (attr==NULL) ) {
        dev_err(dev, "denverton_ns_store_test_hex NULL pointer to private data failure\n");
        return -EOPNOTSUPP;
    }

    res = kstrtouint(buf, 16, &val);
    if(res)
        return res;

    /* For non-standard attributes, have to match the 'name' to determine which one  being accessed */
    if(strcmp("fan_test_alarms",attr->dev_attr.attr.name)==0) {
        mutex_lock(&p_denverton_data->update_lock);
        p_denverton_data->fan_test_alarm_bits = val;
        mutex_unlock(&p_denverton_data->update_lock);
    }
    else if(strcmp("fan_test_faults",attr->dev_attr.attr.name)==0) {
        mutex_lock(&p_denverton_data->update_lock);
        p_denverton_data->fan_test_fault_bits = val;
        mutex_unlock(&p_denverton_data->update_lock);
    }
    else if(strcmp("in_test_pwrgood",attr->dev_attr.attr.name)==0) {
        mutex_lock(&p_denverton_data->update_lock);
        p_denverton_data->pwr_test_bits = val;
        mutex_unlock(&p_denverton_data->update_lock);
    }
    else{
        return -EOPNOTSUPP;
    }

    // update data now since alarms might change based on tests
    res = ucext_dev_denverton_update(dev, p_denverton_data);
    if(res) {
        dev_err(dev, "denverton_ns_store_test_hex call to ucext_dev_denverton_update failed\n");
        return -ENODEV;
    }

    return count;
}

#endif

/*------------------------------------------------------------------------------------------------------------------*/
/* HWMON Non-standard attributes and group entries  */
/* This is kind of a hack but in order to scale non-standard attributes it's easier to define more than you need    */
/* and then control the visibilty to using the defined values.  You just need to make sure you have enough defined. */

static DEVICE_ATTR(fw_major,  S_IRUGO, denverton_ns_show_chip, 0);
static DEVICE_ATTR(fw_minor,  S_IRUGO, denverton_ns_show_chip, 0);
static DEVICE_ATTR(msg_protocol,  S_IRUGO, denverton_ns_show_chip, 0);
static DEVICE_ATTR(hw_id,  S_IRUGO, denverton_ns_show_chip, 0);

#if defined(UCEXT_DENVERTON_DEVICE_INDEX) && (UCEXT_DENVERTON_DEVICE_INDEX==0)
static DEVICE_ATTR(boot_major,  S_IRUGO, denverton_ns_show_chip, 0);
static DEVICE_ATTR(boot_minor,  S_IRUGO, denverton_ns_show_chip, 0);
static DEVICE_ATTR(clk1_major,  S_IRUGO, denverton_ns_show_chip, 0);
static DEVICE_ATTR(clk1_minor,  S_IRUGO, denverton_ns_show_chip, 0);
static DEVICE_ATTR(msg_debug, (S_IWUSR | S_IRUGO), denverton_ns_show_chip, denverton_ns_store_chip);
static DEVICE_ATTR(reset_cause,  S_IRUGO, denverton_ns_show_chip, 0);
static DEVICE_ATTR(reboot, (S_IWUSR), NULL, denverton_ns_store_chip);
static DEVICE_ATTR(watchdog, (S_IWUSR | S_IRUGO), denverton_ns_show_chip, denverton_ns_store_chip);
#endif

static DEVICE_ATTR(dev_status,  S_IRUGO, denverton_ns_show_chip, 0);

#if defined(UCEXT_DENVERTON_DEVICE_INDEX) && (UCEXT_DENVERTON_DEVICE_INDEX==0)
static DEVICE_ATTR(aux_dev1,  S_IRUGO, denverton_ns_show_chip, 0);
static DEVICE_ATTR(aux_dev2,  S_IRUGO, denverton_ns_show_chip, 0);
static DEVICE_ATTR(aux_dev3,  S_IRUGO, denverton_ns_show_chip, 0);
static DEVICE_ATTR(aux_dev4,  S_IRUGO, denverton_ns_show_chip, 0);
static DEVICE_ATTR(aux_dev5,  S_IRUGO, denverton_ns_show_chip, 0);
static DEVICE_ATTR(aux_dev6,  S_IRUGO, denverton_ns_show_chip, 0);
static DEVICE_ATTR(aux_clk1,  S_IRUGO, denverton_ns_show_chip, 0);
static DEVICE_ATTR(aux_prog1,  S_IRUGO, denverton_ns_show_chip, 0);
static DEVICE_ATTR(aux_therm1,  S_IRUGO, denverton_ns_show_chip, 0);
static DEVICE_ATTR(aux_therm2,  S_IRUGO, denverton_ns_show_chip, 0);
static DEVICE_ATTR(aux_therm3,  S_IRUGO, denverton_ns_show_chip, 0);
static DEVICE_ATTR(aux_therm4,  S_IRUGO, denverton_ns_show_chip, 0);
static DEVICE_ATTR(aux_therm5,  S_IRUGO, denverton_ns_show_chip, 0);
static DEVICE_ATTR(aux_therm6,  S_IRUGO, denverton_ns_show_chip, 0);
static DEVICE_ATTR(aux_therm7,  S_IRUGO, denverton_ns_show_chip, 0);
static DEVICE_ATTR(aux_therm8,  S_IRUGO, denverton_ns_show_chip, 0);
static DEVICE_ATTR(aux_therm9,  S_IRUGO, denverton_ns_show_chip, 0);
static DEVICE_ATTR(aux_therm10, S_IRUGO, denverton_ns_show_chip, 0);
static DEVICE_ATTR(dev_rxpkts,  S_IRUGO, denverton_ns_show_chip, 0);
static DEVICE_ATTR(dev_txpkts,  S_IRUGO, denverton_ns_show_chip, 0);
#endif

#if defined(UCEXT_DENVERTON_SYSTEM_FAN) && (UCEXT_DENVERTON_SYSTEM_FAN>0)
static DEVICE_ATTR(fan_input,  S_IRUGO, denverton_ns_show_chip, 0);
static DEVICE_ATTR(fan_alarm,  S_IRUGO, denverton_ns_show_chip, 0);
static DEVICE_ATTR(fan_fault,  S_IRUGO, denverton_ns_show_chip, 0);
#endif

#if defined(NUM_UCEXT_DENVERTON_FANS) && (NUM_UCEXT_DENVERTON_FANS>0)
static DEVICE_ATTR(fan_test_alarms, (S_IWUSR | S_IRUGO), denverton_ns_show_test_hex, denverton_ns_store_test_hex);
static DEVICE_ATTR(fan_test_faults, (S_IWUSR | S_IRUGO), denverton_ns_show_test_hex, denverton_ns_store_test_hex);
#endif

#if defined(NUM_UCEXT_DENVERTON_VOLTS) && (NUM_UCEXT_DENVERTON_VOLTS>0) && defined(UCEXT_DENVERTON_PWRGOOD) && (UCEXT_DENVERTON_PWRGOOD>0)
static DEVICE_ATTR(in_test_pwrgood, (S_IWUSR | S_IRUGO), denverton_ns_show_test_hex, denverton_ns_store_test_hex);
static DEVICE_ATTR(in_pwrgood, (S_IRUGO), denverton_ns_show_test_hex, 0 );
#endif

#if defined(UCEXT_DENVERTON_VIN_ALARMS) && (UCEXT_DENVERTON_VIN_ALARMS>0)
static DEVICE_ATTR(vin1_alarm, S_IRUGO, denverton_ns_show_chip, 0);
static DEVICE_ATTR(vin2_alarm, S_IRUGO, denverton_ns_show_chip, 0);
#endif

static struct attribute *denverton_ns_chip_attrs[] = {
    &dev_attr_fw_major.attr,
    &dev_attr_fw_minor.attr,
    &dev_attr_msg_protocol.attr,
    &dev_attr_hw_id.attr,
#if defined(UCEXT_DENVERTON_DEVICE_INDEX) && (UCEXT_DENVERTON_DEVICE_INDEX==0)
    &dev_attr_boot_major.attr,
    &dev_attr_boot_minor.attr,
    &dev_attr_msg_debug.attr,
    &dev_attr_clk1_major.attr,
    &dev_attr_clk1_minor.attr,
    &dev_attr_reset_cause.attr,
    &dev_attr_reboot.attr,
    &dev_attr_watchdog.attr,
#endif
    &dev_attr_dev_status.attr,
#if defined(UCEXT_DENVERTON_DEVICE_INDEX) && (UCEXT_DENVERTON_DEVICE_INDEX==0)
    &dev_attr_aux_dev1.attr,
    &dev_attr_aux_dev2.attr,
    &dev_attr_aux_dev3.attr,
    &dev_attr_aux_dev4.attr,
    &dev_attr_aux_dev5.attr,
    &dev_attr_aux_dev6.attr,
    &dev_attr_aux_clk1.attr,
    &dev_attr_aux_prog1.attr,
    &dev_attr_aux_therm1.attr,
    &dev_attr_aux_therm2.attr,
    &dev_attr_aux_therm3.attr,
    &dev_attr_aux_therm4.attr,
    &dev_attr_aux_therm5.attr,
    &dev_attr_aux_therm6.attr,
    &dev_attr_aux_therm7.attr,
    &dev_attr_aux_therm8.attr,
    &dev_attr_aux_therm9.attr,
    &dev_attr_aux_therm10.attr,
    &dev_attr_dev_rxpkts.attr,
    &dev_attr_dev_txpkts.attr,
#endif
    // Maintain aggregate system fan avg and alarm/fault attributes
#if defined(UCEXT_DENVERTON_SYSTEM_FAN) && (UCEXT_DENVERTON_SYSTEM_FAN>0)
    &dev_attr_fan_input.attr,
    &dev_attr_fan_alarm.attr,
    &dev_attr_fan_fault.attr,
#endif
#if defined(NUM_UCEXT_DENVERTON_FANS) && (NUM_UCEXT_DENVERTON_FANS>0)
    &dev_attr_fan_test_alarms.attr,
    &dev_attr_fan_test_faults.attr,
#endif
#if defined(NUM_UCEXT_DENVERTON_VOLTS) && (NUM_UCEXT_DENVERTON_VOLTS>0) && defined(UCEXT_DENVERTON_PWRGOOD) && (UCEXT_DENVERTON_PWRGOOD>0)
    &dev_attr_in_test_pwrgood.attr,
    &dev_attr_in_pwrgood.attr,
#endif
#if defined(UCEXT_DENVERTON_VIN_ALARMS) && (UCEXT_DENVERTON_VIN_ALARMS>0)
    &dev_attr_vin1_alarm.attr,
    &dev_attr_vin2_alarm.attr,
#endif
    NULL
};

static const struct attribute_group denverton_ns_chip_group = {
    .attrs = denverton_ns_chip_attrs,
    .is_visible = denverton_ns_chip_attrs_visible,
};

#if defined(NUM_UCEXT_DENVERTON_LEDS) && (NUM_UCEXT_DENVERTON_LEDS>0)

    #if defined(NUM_UCEXT_DENVERTON_LEDS) && (NUM_UCEXT_DENVERTON_LEDS>8)
        #error "NUM_UCEXT_DENVERTON_LED requested exceeds current SENSOR_DEVICE_ATTR supported by ucext_dev_denverton"
    #endif

static SENSOR_DEVICE_ATTR(led1_color, S_IWUSR | S_IRUGO, denverton_ns_show_led_color, denverton_ns_store_led_color, 0);
static SENSOR_DEVICE_ATTR(led2_color, S_IWUSR | S_IRUGO, denverton_ns_show_led_color, denverton_ns_store_led_color, 1);
static SENSOR_DEVICE_ATTR(led3_color, S_IWUSR | S_IRUGO, denverton_ns_show_led_color, denverton_ns_store_led_color, 2);
static SENSOR_DEVICE_ATTR(led4_color, S_IWUSR | S_IRUGO, denverton_ns_show_led_color, denverton_ns_store_led_color, 3);
static SENSOR_DEVICE_ATTR(led5_color, S_IWUSR | S_IRUGO, denverton_ns_show_led_color, denverton_ns_store_led_color, 4);
static SENSOR_DEVICE_ATTR(led6_color, S_IWUSR | S_IRUGO, denverton_ns_show_led_color, denverton_ns_store_led_color, 5);
static SENSOR_DEVICE_ATTR(led7_color, S_IWUSR | S_IRUGO, denverton_ns_show_led_color, denverton_ns_store_led_color, 6);
static SENSOR_DEVICE_ATTR(led8_color, S_IWUSR | S_IRUGO, denverton_ns_show_led_color, denverton_ns_store_led_color, 7);


static struct attribute *denverton_ns_led_color_attrs[] = {
    &sensor_dev_attr_led1_color.dev_attr.attr,
    &sensor_dev_attr_led2_color.dev_attr.attr,
    &sensor_dev_attr_led3_color.dev_attr.attr,
    &sensor_dev_attr_led4_color.dev_attr.attr,
    &sensor_dev_attr_led5_color.dev_attr.attr,
    &sensor_dev_attr_led6_color.dev_attr.attr,
    &sensor_dev_attr_led7_color.dev_attr.attr,
    &sensor_dev_attr_led8_color.dev_attr.attr,
    NULL
};

static const struct attribute_group denverton_ns_led_color_group = {
    .attrs = denverton_ns_led_color_attrs,
    .is_visible = denverton_ns_rw_led_attrs_visible,
};


static SENSOR_DEVICE_ATTR(led1_rate,  S_IWUSR | S_IRUGO, denverton_ns_show_led_rate, denverton_ns_store_led_rate, 0);
static SENSOR_DEVICE_ATTR(led2_rate,  S_IWUSR | S_IRUGO, denverton_ns_show_led_rate, denverton_ns_store_led_rate, 1);
static SENSOR_DEVICE_ATTR(led3_rate,  S_IWUSR | S_IRUGO, denverton_ns_show_led_rate, denverton_ns_store_led_rate, 2);
static SENSOR_DEVICE_ATTR(led4_rate,  S_IWUSR | S_IRUGO, denverton_ns_show_led_rate, denverton_ns_store_led_rate, 3);
static SENSOR_DEVICE_ATTR(led5_rate,  S_IWUSR | S_IRUGO, denverton_ns_show_led_rate, denverton_ns_store_led_rate, 4);
static SENSOR_DEVICE_ATTR(led6_rate,  S_IWUSR | S_IRUGO, denverton_ns_show_led_rate, denverton_ns_store_led_rate, 5);
static SENSOR_DEVICE_ATTR(led7_rate,  S_IWUSR | S_IRUGO, denverton_ns_show_led_rate, denverton_ns_store_led_rate, 6);
static SENSOR_DEVICE_ATTR(led8_rate,  S_IWUSR | S_IRUGO, denverton_ns_show_led_rate, denverton_ns_store_led_rate, 7);

static struct attribute *denverton_ns_led_rate_attrs[] = {
    &sensor_dev_attr_led1_rate.dev_attr.attr,
    &sensor_dev_attr_led2_rate.dev_attr.attr,
    &sensor_dev_attr_led3_rate.dev_attr.attr,
    &sensor_dev_attr_led4_rate.dev_attr.attr,
    &sensor_dev_attr_led5_rate.dev_attr.attr,
    &sensor_dev_attr_led6_rate.dev_attr.attr,
    &sensor_dev_attr_led7_rate.dev_attr.attr,
    &sensor_dev_attr_led8_rate.dev_attr.attr,
    NULL
};

static const struct attribute_group denverton_ns_led_rate_group = {
    .attrs = denverton_ns_led_rate_attrs,
    .is_visible = denverton_ns_rw_led_attrs_visible,
};
#endif


#if defined(NUM_UCEXT_DENVERTON_BUTTONS) && (NUM_UCEXT_DENVERTON_BUTTONS>0)

    #if defined(NUM_UCEXT_DENVERTON_BUTTONS) && (NUM_UCEXT_DENVERTON_BUTTONS>3)
        #error "NUM_UCEXT_DENVERTON_BUTTONS requested exceeds current SENSOR_DEVICE_ATTR supported by ucext_dev_denverton"
    #endif

static SENSOR_DEVICE_ATTR(button1, S_IWUSR | S_IRUGO, denverton_ns_show_button, denverton_ns_store_button, 0);
static SENSOR_DEVICE_ATTR(button2, S_IWUSR | S_IRUGO, denverton_ns_show_button, denverton_ns_store_button, 1);
static SENSOR_DEVICE_ATTR(button3, S_IWUSR | S_IRUGO, denverton_ns_show_button, denverton_ns_store_button, 2);

static struct attribute *denverton_ns_button_attrs[] = {
    &sensor_dev_attr_button1.dev_attr.attr,
    &sensor_dev_attr_button2.dev_attr.attr,
    &sensor_dev_attr_button3.dev_attr.attr,
    NULL
};

static const struct attribute_group denverton_ns_button_group = {
    .attrs = denverton_ns_button_attrs,
    .is_visible = denverton_ns_rw_button_attrs_visible,
};

#endif

/*------------------------------------------------------------------------------------------------------------------*/
/* HWMON standard group structs for registration.                                           */
/* groups aggregation of each group (to allow number of attribute scaling using is_visible) */

static const struct attribute_group *denverton_ns_groups[] = {  
    &denverton_ns_chip_group,
#if defined(NUM_UCEXT_DENVERTON_LEDS) && (NUM_UCEXT_DENVERTON_LEDS>0)
    &denverton_ns_led_color_group,
    &denverton_ns_led_rate_group,
#endif
#if defined(NUM_UCEXT_DENVERTON_BUTTONS) && (NUM_UCEXT_DENVERTON_BUTTONS>0)
    &denverton_ns_button_group,
#endif
    NULL,   
};

static ssize_t show_ns_sensor_label(struct device *dev,
                                    struct device_attribute *da, char *buf)
{
    struct sensors_label *sl = to_label_attr(da);
    return snprintf(buf, PAGE_SIZE, "%s\n", sl->attr_label);
}

static ssize_t store_ns_sensor_label(struct device *dev, 
                                     struct device_attribute *da, const char *buf, size_t count)
{
    struct sensors_label *la = to_label_attr(da);
    ssize_t cnt = count;
    if(la->attr_wrcnt==0) {
        cnt = snprintf(la->attr_label, sizeof(la->attr_label), "%s", buf);
        // Keep only up to the first CR or LF  and remove it and anything after it.
        if(cnt>0) {
            la->attr_label[strcspn(la->attr_label, "\r\n")] = 0;
            la->attr_wrcnt++;
            cnt = count;
        }
        else {
            cnt = -EINVAL;
        }
    }
    else {
        cnt = -EPERM;
    }

    return cnt;
}


/*------------------------------------------------------------------------------------------------------------------*/
/* DENVERTON Multi-Purpose Device subsystem and data structure init and exit functions */
/* returns a handle to it's private data structure  or NULL on failure*/
void *ucext_dev_denverton_init(struct device *dev)
{
    struct denverton_data *p_denverton_data;
    void *drvdata;  
    int i;
    int idx;
    int res;
    void *p_msg_data;
    struct sensors_label *p_label;

    if(dev==NULL) {
        dev_err(dev, "ucext_dev_denverton_init NULL parent device pointer\n");
        return NULL;
    }

#if (!defined(UCEXT_DENVERTON_DEVM_MEM_ALLOC) || (UCEXT_DENVERTON_DEVM_MEM_ALLOC==0))
    p_denverton_data = kzalloc(sizeof(struct denverton_data), GFP_KERNEL);
#else
    p_denverton_data = devm_kzalloc(dev, sizeof(struct denverton_data), GFP_KERNEL);
#endif

    if(!p_denverton_data) {
        dev_err(dev, "ucext_dev_denverton_init failed to allocate private message data\n");
        return NULL;
    }
    p_denverton_data->valid = 0;
    p_denverton_data->dev = dev;

    mutex_init(&p_denverton_data->update_lock);

    mutex_lock(&p_denverton_data->update_lock);

    /* Init to zero to force initial first update */
    p_denverton_data->last_updated = 0;   

    p_denverton_data->chip_update_interval = UCEXT_DENVERTON_MIN_UPDATE_INTERVAL;
    p_denverton_data->chip_fw_major = 0;   
    p_denverton_data->chip_fw_minor = 0;   
    p_denverton_data->chip_protocol = 0;
    p_denverton_data->chip_hw_id = 0;   

#if defined(UCEXT_DENVERTON_DEVICE_INDEX) && (UCEXT_DENVERTON_DEVICE_INDEX==0)
    p_denverton_data->chip_boot_major = 0;   
    p_denverton_data->chip_boot_minor = 0;
    p_denverton_data->chip_num_pwms = 0;   
    p_denverton_data->chip_num_fans = 0;   
    p_denverton_data->chip_num_temps = 0;   
    p_denverton_data->chip_num_auxilary = 0;   
     
    p_denverton_data->chip_reset_cause = 0;   
    p_denverton_data->chip_reboot = 0;   
    p_denverton_data->chip_watchdog = 0;   
#endif

    /* Init the chip/aux status values */
    p_denverton_data->chip_dev_status = 0;
#if defined(UCEXT_DENVERTON_DEVICE_INDEX) && (UCEXT_DENVERTON_DEVICE_INDEX==0)
    p_denverton_data->chip_aux_dev1 = 0;
    p_denverton_data->chip_aux_dev2 = 0;
    p_denverton_data->chip_aux_dev3 = 0;
    p_denverton_data->chip_aux_dev4 = 0;
    p_denverton_data->chip_aux_dev5 = 0;
    p_denverton_data->chip_aux_dev6 = 0;
    p_denverton_data->chip_aux_clk1 = 0;
    p_denverton_data->chip_aux_prog1 = 0;
    p_denverton_data->chip_aux_therm1 = 0;
    p_denverton_data->chip_aux_therm2 = 0;
    p_denverton_data->chip_aux_therm3 = 0;
    p_denverton_data->chip_aux_therm4 = 0;
    p_denverton_data->chip_aux_therm5 = 0;
    p_denverton_data->chip_aux_therm6 = 0;
    p_denverton_data->chip_aux_therm7 = 0;
    p_denverton_data->chip_aux_therm8 = 0;
    p_denverton_data->chip_aux_therm9 = 0;
    p_denverton_data->chip_aux_therm10 = 0;
    p_denverton_data->chip_dev_rxpkts = 0;
    p_denverton_data->chip_dev_txpkts = 0;
#endif

    for(i=0;i<NUM_UCEXT_DENVERTON_TEMPS;i++) {
        p_denverton_data->temp_in_vals[i] = 0;

        p_denverton_data->temp_mins[i] = -40000;
        p_denverton_data->temp_min_hysts[i] = -35000;
        p_denverton_data->temp_max_hysts[i] = 80000;
        p_denverton_data->temp_maxs[i] = 85000;
        p_denverton_data->temp_crit_hysts[i] = 90000;
        p_denverton_data->temp_crits[i] = 95000;
        p_denverton_data->temp_emerg_hysts[i] = 95000;
        p_denverton_data->temp_emergs[i] = 100000;

        p_denverton_data->temp_min_alarms[i] = 0;
        p_denverton_data->temp_max_alarms[i] = 0;
        p_denverton_data->temp_crit_alarms[i] = 0;
        p_denverton_data->temp_emerg_alarms[i] = 0;
    }

    for(i=0;i<NUM_UCEXT_DENVERTON_CURRS;i++) {
        p_denverton_data->curr_in_vals[i] = 0;
    }

    for(i=0;i<NUM_UCEXT_DENVERTON_FANS;i++) {
        p_denverton_data->fan_in_vals[i] = 0;
        p_denverton_data->fan_present[i] = 1;
        p_denverton_data->fan_faults[i] = 0;
        p_denverton_data->fan_alarms[i] = 0;
    }
    p_denverton_data->fan_test_alarm_bits = 0x0000;
    p_denverton_data->fan_test_fault_bits = 0x0000;


#if defined(UCEXT_DENVERTON_SYSTEM_FAN) && (UCEXT_DENVERTON_SYSTEM_FAN!=0)
    p_denverton_data->system_fan_avg = 0;
    p_denverton_data->system_fan_alarm = 0;
    p_denverton_data->system_fan_fault = 0;
#endif

    /* TBD for now default PWMs like denverton till we can read these values */
    for(i=0;i<NUM_UCEXT_DENVERTON_PWMS;i++) {
        p_denverton_data->pwm_in_vals[i] = 64;
        p_denverton_data->pwm_en_vals[i] = 2;
    }

    for(i=0;i<NUM_UCEXT_DENVERTON_VOLTS;i++) {
        p_denverton_data->volt_in_vals[i] = 0;
        p_denverton_data->volt_maxs[i] = 100000;
        p_denverton_data->volt_mins[i] = 0;
        p_denverton_data->volt_lcrits[i] = 0;
        p_denverton_data->volt_max_alarms[i] = 0;
        p_denverton_data->volt_min_alarms[i] = 0;
        p_denverton_data->volt_lcrit_alarms[i] = 0;
    }

    /* Init all power indications good to start (Only 16 bits for now) */
    p_denverton_data->pwr_good_bits = 0xffff;
    p_denverton_data->pwr_test_bits = 0x0000;

    for(i=0;i<NUM_UCEXT_DENVERTON_LEDS;i++) {
        p_denverton_data->led_color_vals[i] = 0;
        p_denverton_data->led_rate_vals[i] = 0;
    }
    /* TBD for now default power LED to green like denverton till we can read these values */
    p_denverton_data->led_color_vals[0] = 2;


#if (!defined(UCEXT_DENVERTON_DEVM_MEM_ALLOC) || (UCEXT_DENVERTON_DEVM_MEM_ALLOC==0))
    p_denverton_data->p_sensor_label_data = kzalloc(          ((NUM_UCEXT_DENVERTON_CURRS +
                                                              NUM_UCEXT_DENVERTON_FANS  +
                                                              UCEXT_DENVERTON_SYSTEM_FAN +
                                                              NUM_UCEXT_DENVERTON_PWMS  +
                                                              NUM_UCEXT_DENVERTON_TEMPS +
                                                              NUM_UCEXT_DENVERTON_VOLTS +
                                                              NUM_UCEXT_DENVERTON_LEDS +
                                                              NUM_UCEXT_DENVERTON_BUTTONS + 1)*sizeof(struct sensors_label)), 
                                                            GFP_KERNEL);
    p_denverton_data->p_sensor_label_attrs = kzalloc(         ((NUM_UCEXT_DENVERTON_CURRS +
                                                              NUM_UCEXT_DENVERTON_FANS  +
                                                              UCEXT_DENVERTON_SYSTEM_FAN +
                                                              NUM_UCEXT_DENVERTON_PWMS  +
                                                              NUM_UCEXT_DENVERTON_TEMPS +
                                                              NUM_UCEXT_DENVERTON_VOLTS +
                                                              NUM_UCEXT_DENVERTON_LEDS +
                                                              NUM_UCEXT_DENVERTON_BUTTONS + 1)*sizeof(struct attribute *)), 
                                                            GFP_KERNEL);
#else
    p_denverton_data->p_sensor_label_data = devm_kzalloc(dev, ((NUM_UCEXT_DENVERTON_CURRS +
                                                              NUM_UCEXT_DENVERTON_FANS  +
                                                              UCEXT_DENVERTON_SYSTEM_FAN +
                                                              NUM_UCEXT_DENVERTON_PWMS  +
                                                              NUM_UCEXT_DENVERTON_TEMPS +
                                                              NUM_UCEXT_DENVERTON_VOLTS +
                                                              NUM_UCEXT_DENVERTON_LEDS +
                                                              NUM_UCEXT_DENVERTON_BUTTONS + 1)*sizeof(struct sensors_label)), 
                                                       GFP_KERNEL);
    p_denverton_data->p_sensor_label_attrs = devm_kzalloc(dev,((NUM_UCEXT_DENVERTON_CURRS +
                                                              NUM_UCEXT_DENVERTON_FANS  +
                                                              UCEXT_DENVERTON_SYSTEM_FAN +
                                                              NUM_UCEXT_DENVERTON_PWMS  +
                                                              NUM_UCEXT_DENVERTON_TEMPS +
                                                              NUM_UCEXT_DENVERTON_VOLTS +
                                                              NUM_UCEXT_DENVERTON_LEDS +
                                                              NUM_UCEXT_DENVERTON_BUTTONS + 1)*sizeof(struct attribute *)), 
                                                        GFP_KERNEL);
#endif

    if(!p_denverton_data->p_sensor_label_data || !p_denverton_data->p_sensor_label_attrs) {
        dev_err(dev, "ucext_dev_denverton_init dynamic label allocation failed!\n");
        goto unlock_error;
    }

    p_label = p_denverton_data->p_sensor_label_data;
    idx = 0;
    for(i = 0; i < NUM_UCEXT_DENVERTON_CURRS; i++) {
        snprintf(p_label->attr_name , sizeof(p_label->attr_name), "curr%d_label", (i+1) );
        snprintf(p_label->attr_label , sizeof(p_label->attr_label), "CURR #%d", (i+1) );
//        p_label->attr_label[0] = '\0';
        p_label->attr_wrcnt = 0;
        sysfs_attr_init(&p_label->dev_attr_label.attr);
        p_label->dev_attr_label.attr.name = p_label->attr_name;
        p_label->dev_attr_label.attr.mode = (S_IWUSR | S_IRUGO);
        p_label->dev_attr_label.show = &show_ns_sensor_label;
        p_label->dev_attr_label.store = &store_ns_sensor_label;
        /* Add a pointer the the device atribute list */
        p_denverton_data->p_sensor_label_attrs[idx]=&p_label->dev_attr_label.attr;
        p_label++;
        idx++;
    }

    for(i = 0; i < NUM_UCEXT_DENVERTON_FANS; i++) {
        snprintf(p_label->attr_name , sizeof(p_label->attr_name), "fan%d_label", (i+1) );
        snprintf(p_label->attr_label , sizeof(p_label->attr_label), "FAN #%d", (i+1) );
//        p_label->attr_label[0] = '\0';
        p_label->attr_wrcnt = 0;
        sysfs_attr_init(&p_label->dev_attr_label.attr);
        p_label->dev_attr_label.attr.name = p_label->attr_name;
        p_label->dev_attr_label.attr.mode = (S_IWUSR | S_IRUGO);
        p_label->dev_attr_label.show = &show_ns_sensor_label;
        p_label->dev_attr_label.store = &store_ns_sensor_label;
        /* Add a pointer the the device atribute list */
        p_denverton_data->p_sensor_label_attrs[idx]=&p_label->dev_attr_label.attr;
        p_label++;
        idx++;
    }

#if defined(UCEXT_DENVERTON_SYSTEM_FAN) && (UCEXT_DENVERTON_SYSTEM_FAN!=0)
    snprintf(p_label->attr_name , sizeof(p_label->attr_name), "fan_label" );
    snprintf(p_label->attr_label , sizeof(p_label->attr_label), "FAN SYSTEM" );
//        p_label->attr_label[0] = '\0';
    p_label->attr_wrcnt = 0;
    sysfs_attr_init(&p_label->dev_attr_label.attr);
    p_label->dev_attr_label.attr.name = p_label->attr_name;
    p_label->dev_attr_label.attr.mode = (S_IWUSR | S_IRUGO);
    p_label->dev_attr_label.show = &show_ns_sensor_label;
    p_label->dev_attr_label.store = &store_ns_sensor_label;
    /* Add a pointer the the device atribute list */
    p_denverton_data->p_sensor_label_attrs[idx]=&p_label->dev_attr_label.attr;
    p_label++;
    idx++;
#endif

    for(i = 0; i < NUM_UCEXT_DENVERTON_PWMS; i++) {
        snprintf(p_label->attr_name , sizeof(p_label->attr_name), "pwm%d_label", (i+1) );
        snprintf(p_label->attr_label , sizeof(p_label->attr_label), "PWM #%d", (i+1) );
//        p_label->attr_label[0] = '\0';
        p_label->attr_wrcnt = 0;
        sysfs_attr_init(&p_label->dev_attr_label.attr);
        p_label->dev_attr_label.attr.name = p_label->attr_name;
        p_label->dev_attr_label.attr.mode = (S_IWUSR | S_IRUGO);
        p_label->dev_attr_label.show = &show_ns_sensor_label;
        p_label->dev_attr_label.store = &store_ns_sensor_label;
        /* Add a pointer the the device atribute list */
        p_denverton_data->p_sensor_label_attrs[idx]=&p_label->dev_attr_label.attr;
        p_label++;
        idx++;
    }

    for(i = 0; i < NUM_UCEXT_DENVERTON_TEMPS; i++) {
        snprintf(p_label->attr_name , sizeof(p_label->attr_name), "temp%d_label", (i+1) );
        snprintf(p_label->attr_label , sizeof(p_label->attr_label), "TEMP #%d", (i+1) );
//        p_label->attr_label[0] = '\0';
        p_label->attr_wrcnt = 0;
        sysfs_attr_init(&p_label->dev_attr_label.attr);
        p_label->dev_attr_label.attr.name = p_label->attr_name; 
        p_label->dev_attr_label.attr.mode = (S_IWUSR | S_IRUGO);
        p_label->dev_attr_label.show = &show_ns_sensor_label;
        p_label->dev_attr_label.store = &store_ns_sensor_label;
        /* Add a pointer the the device atribute list */
        p_denverton_data->p_sensor_label_attrs[idx]=&p_label->dev_attr_label.attr;
        p_label++;
        idx++;
    }

    /* NOTE: Voltages index base 0 instead of 1 for some stupid reason in spec for HWMON Sysfs! */
    for(i = 0; i < NUM_UCEXT_DENVERTON_VOLTS; i++) {
        snprintf(p_label->attr_name , sizeof(p_label->attr_name), "in%d_label", (i) );
        snprintf(p_label->attr_label , sizeof(p_label->attr_label), "IN #%d", (i) );
//        p_label->attr_label[0] = '\0';
        p_label->attr_wrcnt = 0;
        sysfs_attr_init(&p_label->dev_attr_label.attr);
        p_label->dev_attr_label.attr.name = p_label->attr_name;
        p_label->dev_attr_label.attr.mode = (S_IWUSR | S_IRUGO);
        p_label->dev_attr_label.show = &show_ns_sensor_label;
        p_label->dev_attr_label.store = &store_ns_sensor_label;
        /* Add a pointer the the device atribute list */
        p_denverton_data->p_sensor_label_attrs[idx]=&p_label->dev_attr_label.attr;
        p_label++;
        idx++;
    }

    for(i = 0; i < NUM_UCEXT_DENVERTON_LEDS; i++) {
        snprintf(p_label->attr_name , sizeof(p_label->attr_name), "led%d_label", (i+1) );
        snprintf(p_label->attr_label , sizeof(p_label->attr_label), "LED #%d", (i+1) );
//        p_label->attr_label[0] = '\0';
        p_label->attr_wrcnt = 0;
        sysfs_attr_init(&p_label->dev_attr_label.attr);
        p_label->dev_attr_label.attr.name = p_label->attr_name;
        p_label->dev_attr_label.attr.mode = (S_IWUSR | S_IRUGO);
        p_label->dev_attr_label.show = &show_ns_sensor_label;
        p_label->dev_attr_label.store = &store_ns_sensor_label;
        /* Add a pointer the the device atribute list */
        p_denverton_data->p_sensor_label_attrs[idx]=&p_label->dev_attr_label.attr;
        p_label++;
        idx++;
    }

    for(i = 0; i < NUM_UCEXT_DENVERTON_BUTTONS; i++) {
        snprintf(p_label->attr_name , sizeof(p_label->attr_name), "button%d_label", (i+1) );
        snprintf(p_label->attr_label , sizeof(p_label->attr_label), "BUTTON #%d", (i+1) );
//        p_label->attr_label[0] = '\0';
        p_label->attr_wrcnt = 0;
        sysfs_attr_init(&p_label->dev_attr_label.attr);
        p_label->dev_attr_label.attr.name = p_label->attr_name;
        p_label->dev_attr_label.attr.mode = (S_IWUSR | S_IRUGO);
        p_label->dev_attr_label.show = &show_ns_sensor_label;
        p_label->dev_attr_label.store = &store_ns_sensor_label;
        /* Add a pointer the the device atribute list */
        p_denverton_data->p_sensor_label_attrs[idx]=&p_label->dev_attr_label.attr;
        p_label++;
        idx++;
    }

    /* Create the group of label attributs  */
    p_denverton_data->label_group.attrs = p_denverton_data->p_sensor_label_attrs;

    /* Copy the denverton_ns_groups built with HWMON MACROS to the dynamic list of attribute groups till find NULL termination  */
    i=0;
    while(denverton_ns_groups[i]!=NULL) {
        p_denverton_data->denverton_attr_groups[i] = denverton_ns_groups[i];
        if(++i>=15)
            break;
    }
    /* Append custom ns label group to the dynamic list of attribute groups   */
    p_denverton_data->denverton_attr_groups[i++] = &p_denverton_data->label_group;
    p_denverton_data->denverton_attr_groups[i] = NULL;

    /* Register the provided device by info and possible groups with HWMON */
    drvdata = dev_get_drvdata(dev);
#if (!defined(UCEXT_DENVERTON_DEVM_MEM_ALLOC) || (UCEXT_DENVERTON_DEVM_MEM_ALLOC==0))
    // Non-device managed memory allocations
    p_denverton_data->dev_hwmon = hwmon_device_register_with_info(dev, "msp430denverton", drvdata,
                                                                &ucext_chip_info, p_denverton_data->denverton_attr_groups);
#else
    // Auto device managed memory allocations
    p_denverton_data->dev_hwmon = devm_hwmon_device_register_with_info(dev, "msp430denverton", drvdata,
                                                                     &ucext_chip_info, p_denverton_data->denverton_attr_groups);
#endif

    p_msg_data = ucext_get_dev_msg_data(dev);
    if(p_msg_data==NULL) {
        dev_err(dev, "ucext_dev_denverton_init NULL pointer to private msg data\n");
        goto unlock_error;
    }

    p_denverton_data->valid = 1;
    mutex_unlock(&p_denverton_data->update_lock);

//#if defined(UCEXT_DENVERTON_DEVICE_INDEX) && (UCEXT_DENVERTON_DEVICE_INDEX==0)
//    res =  ucext_msg_set_msg_protocol(UCEXT_DENVERTON_DEVICE_INDEX, p_msg_data, 2);
//    if(res) {
//        dev_info(dev, "ucext_dev_denverton_init cmd set protocol failure\n");
//    }
//#endif

    /* Initial Read of DENVERTON device to sync data*/
    res =  ucext_msg_update_config(UCEXT_DENVERTON_DEVICE_INDEX, p_msg_data, &p_denverton_data->chip_fw_major);
    if(res) {
        dev_err(dev, "ucext_dev_denverton_init call to ucext_msg_update_config failed!\n");
        goto error;
    }

#if defined(UCEXT_DENVERTON_DEVICE_INDEX) && (UCEXT_DENVERTON_DEVICE_INDEX==0)
    dev_info(dev, 
             "uController Extend denverton FW=%d.%d,HW=%d,MSG=%d,PWMs=%d,FANs=%d,LEDs=%d,TEMPs=%d,AUXs=%d,RESET=%d\n",
             p_denverton_data->chip_fw_major,
             p_denverton_data->chip_fw_minor,
             p_denverton_data->chip_hw_id,
             p_denverton_data->chip_protocol,
             p_denverton_data->chip_num_pwms,
             p_denverton_data->chip_num_fans,
             p_denverton_data->chip_num_leds,
             p_denverton_data->chip_num_temps,
             p_denverton_data->chip_num_auxilary,
             p_denverton_data->chip_reset_cause
             );
#else
    dev_info(dev, 
             "uController Extend denverton FW=%d.%d,HW=%d\n",
             p_denverton_data->chip_fw_major,
             p_denverton_data->chip_fw_minor,
             p_denverton_data->chip_hw_id
             );
#endif

    res = ucext_dev_denverton_update(dev, p_denverton_data);
    if(res) {
        dev_err(dev, "ucext_dev_denverton_init call to ucext_dev_denverton_update failed!\n");
        goto error;
    }
    else if (ucext_msg_restricted(p_msg_data)) {
            dev_info(dev, "uController Extend denverton device initial data sync restricted access\n");
    } 
    else {
            dev_info(dev, "uController Extend denverton device initial data sync successful\n");
    }

    if(!p_denverton_data->dev_hwmon) {
        dev_err(dev, "ucext_dev_denverton_init failed hwmon device register\n");
        goto error;
    }
    else {
        dev_info(p_denverton_data->dev_hwmon, "uController Extend denverton device HWMON sensor registration successful\n");
    }

    /* Mark the final context safeguard flag as valid now*/
    return p_denverton_data;

    unlock_error:
    mutex_unlock(&p_denverton_data->update_lock);
    error:
#if (!defined(UCEXT_DENVERTON_DEVM_MEM_ALLOC) || (UCEXT_DENVERTON_DEVM_MEM_ALLOC==0))
    kfree (p_denverton_data);
#else
    devm_kfree (dev, p_denverton_data);
#endif
    return NULL;
}


void ucext_dev_denverton_exit(struct device *dev) {
    struct denverton_data *p_denverton_data;

    if(dev==NULL) {
        return;
    }
    dev_info(dev, "ucext_dev_denverton_exit initiated\n");

    p_denverton_data = ucext_get_dev_denverton_data(dev);

    if(p_denverton_data==NULL) {
        return;
    }

#if (!defined(UCEXT_DENVERTON_DEVM_MEM_ALLOC) || (UCEXT_DENVERTON_DEVM_MEM_ALLOC==0))
    // Non-device managed memory allocations
    // When using driver managed devm_hwmon_device_register_with_info and devm_kzalloc, the memory is handled by the device automaticallky
    // through the probe and disconnect.

    // Unregister with HWMON before freeing memory
    if(p_denverton_data->dev_hwmon!=NULL) {
        dev_info(p_denverton_data->dev_hwmon, "ucext_dev_denverton_exit unregister HWMON sysfs\n");
        hwmon_device_unregister(p_denverton_data->dev_hwmon);
    }

    // Free the label dynamically allocated attributes and data
    if(p_denverton_data->p_sensor_label_data!=NULL) {
        dev_info(dev, "ucext_dev_denverton_exit free label sensor private data\n");
        kfree(p_denverton_data->p_sensor_label_data);
    }

    // Free the label dynamically alocated attribute list for the group
    if(p_denverton_data->p_sensor_label_attrs!=NULL) {
        dev_info(dev, "ucext_dev_denverton_exit free label attribute pointer array\n");
        kfree(p_denverton_data->p_sensor_label_attrs);
    }
#endif

    /* Mark the final context safeguard flag as invalid valid now*/
    p_denverton_data->valid = 0;

#if (!defined(UCEXT_DENVERTON_DEVM_MEM_ALLOC) || (UCEXT_DENVERTON_DEVM_MEM_ALLOC==0))
    dev_info(dev, "ucext_dev_denverton_exit free denverton private data\n");
    kfree (p_denverton_data);
#endif
}


int ucext_dev_denverton_update(struct device *dev, struct denverton_data *p_denverton_data) {
    int retval = 0;
    void *p_msg_data;
    unsigned long delta_jiffies;
    int i;
    int prior_val;
#if defined(NUM_UCEXT_DENVERTON_FANS) && (NUM_UCEXT_DENVERTON_FANS>0)
    int actual_fans;
    int non_zero_fans;
    int fan_fault, fan_alarm;
    long int fan_avg;
#endif
#if defined(UCEXT_DENVERTON_PWRGOOD) && (UCEXT_DENVERTON_PWRGOOD>0)
    unsigned int prior_pwrgood_bits;
    unsigned int tmp_pwrgood_bits;
#endif

    if( (dev==NULL) || (p_denverton_data==NULL) ) {
        return -ENODEV;
    }

    p_msg_data = ucext_get_dev_msg_data(dev);

    if(!p_msg_data) {
        dev_err(dev, "ucext_dev_denverton_update null pointer to private message data\n");
        return -ENODEV;
    }

    if(p_denverton_data->valid==0) {
        dev_err(dev, "ucext_dev_denverton_update private denverton data not valid\n");
        return -ENODEV;
    }

    /* If messaging restricted, then re-check the config to see if restriction removed yet */
    if (ucext_msg_restricted(p_msg_data)) {
        retval =  ucext_msg_update_config(UCEXT_DENVERTON_DEVICE_INDEX, p_msg_data, &p_denverton_data->chip_fw_major);
        if(retval) {
            dev_err(dev, "ucext_dev_denverton_update call to ucext_msg_update_config failed!\n");
        }
        /* Just return without error if still restricted */
        if (ucext_msg_restricted(p_msg_data)) {
            return 0; 
        }
    }

    mutex_lock(&p_denverton_data->update_lock);
    delta_jiffies =  (p_denverton_data->chip_update_interval*HZ)/1000;

    if(time_after(jiffies, p_denverton_data->last_updated + delta_jiffies)) {
        p_denverton_data->last_updated = jiffies;

        retval = ucext_msg_update_aux_status(UCEXT_DENVERTON_DEVICE_INDEX, p_msg_data, &p_denverton_data->chip_dev_status);

        if(retval) {
            dev_err(dev, "ucext_dev_denverton_update call to ucext_msg_update_aux_status failed\n");
            goto error;
        }

#if defined(NUM_UCEXT_DENVERTON_TEMPS) && (NUM_UCEXT_DENVERTON_TEMPS>0)
        retval = ucext_msg_update_temps(UCEXT_DENVERTON_DEVICE_INDEX, p_msg_data, p_denverton_data->temp_in_vals);

        if(retval) {
            dev_err(dev, "ucext_dev_denverton_update call to ucext_msg_update_temps failed\n");
            goto error;
        }
#endif

#if defined(UCEXT_DENVERTON_PWRGOOD) && (UCEXT_DENVERTON_PWRGOOD>0)
        prior_pwrgood_bits = p_denverton_data->pwr_good_bits;
#endif

#if defined(NUM_UCEXT_DENVERTON_VOLTS) && (NUM_UCEXT_DENVERTON_VOLTS>0)
        retval = ucext_msg_update_volts(UCEXT_DENVERTON_DEVICE_INDEX, p_msg_data, p_denverton_data->volt_in_vals, 
                                        &p_denverton_data->pwr_good_bits, p_denverton_data->pwr_test_bits);
        if(retval) {
            dev_err(dev, "ucext_dev_denverton_update call to ucext_msg_update_volts failed\n");
            goto error;
        }
#endif

#if defined(UCEXT_DENVERTON_PWRGOOD) && (UCEXT_DENVERTON_PWRGOOD>0)
        tmp_pwrgood_bits = p_denverton_data->pwr_good_bits;
#if defined(UCEXT_DENVERTON_VIN_ALARMS) && (UCEXT_DENVERTON_VIN_ALARMS>0)
        // Don't try to clear VIN ALARM pwr goods bits since these are not from MSP430 power controllers
        tmp_pwrgood_bits |= 0xc000;        
#endif                
        if( ((~tmp_pwrgood_bits)!=0) && (p_denverton_data->pwr_test_bits==0) ) {
            retval = ucext_msg_clear_pwrgood(UCEXT_DENVERTON_DEVICE_INDEX, p_msg_data);
    
            if(retval) {
                dev_err(dev, "ucext_dev_denverton_update call to ucext_msg_clear_pwrgood failed\n");
                goto error;
            }
        }
#endif

#if defined(NUM_UCEXT_DENVERTON_CURRS) && (NUM_UCEXT_DENVERTON_CURRS>0)
        retval = ucext_msg_update_currs(UCEXT_DENVERTON_DEVICE_INDEX, p_msg_data, p_denverton_data->curr_in_vals);

        if(retval) {
            dev_err(dev, "ucext_dev_denverton_update call to ucext_msg_update_currs failed\n");
            goto error;
        }
#endif

#if defined(NUM_UCEXT_DENVERTON_FANS) && (NUM_UCEXT_DENVERTON_FANS>0)
        retval = ucext_msg_update_fans(UCEXT_DENVERTON_DEVICE_INDEX, p_denverton_data->chip_protocol, p_denverton_data->chip_num_fans, p_msg_data, p_denverton_data->fan_in_vals, p_denverton_data->fan_present);

        if(retval) {
            dev_err(dev, "ucext_dev_denverton_update call to ucext_msg_update_fans failed\n");
            goto error;
        }
#endif

#if defined(NUM_UCEXT_DENVERTON_LEDS) && (NUM_UCEXT_DENVERTON_LEDS>0)
        retval = ucext_msg_update_leds(UCEXT_DENVERTON_DEVICE_INDEX, p_denverton_data->chip_protocol, p_denverton_data->chip_num_leds, p_msg_data, p_denverton_data->led_color_vals, p_denverton_data->led_rate_vals);

        if(retval) {
            dev_err(dev, "ucext_dev_denverton_update call to ucext_msg_update_leds failed\n");
            goto error;
        }
#endif

#if defined(NUM_UCEXT_DENVERTON_BUTTONS) && (NUM_UCEXT_DENVERTON_BUTTONS>0)
        retval = ucext_msg_update_buttons(UCEXT_DENVERTON_DEVICE_INDEX, p_msg_data, p_denverton_data->button_vals);

        if(retval) {
            dev_err(dev, "ucext_dev_denverton_update call to ucext_msg_update_buttons failed\n");
            goto error;
        }
#endif

        /* VIN Alarms post processing, check these only on updates since no threshold */
#if defined(UCEXT_DENVERTON_PWRGOOD) && (UCEXT_DENVERTON_PWRGOOD>0) && defined(UCEXT_DENVERTON_VIN_ALARMS) && (UCEXT_DENVERTON_VIN_ALARMS>0)
        if( (p_denverton_data->pwr_good_bits&0x04000)!=(prior_pwrgood_bits&0x04000) ) {
            dev_info(p_denverton_data->dev_hwmon, "ALERT denverton vin1_alarm changed to state %d\n", 
                     (p_denverton_data->pwr_good_bits&0x4000? 0: 1) );
        }
        if( (p_denverton_data->pwr_good_bits&0x08000)!=(prior_pwrgood_bits&0x08000) ) {
            dev_info(p_denverton_data->dev_hwmon, "ALERT denverton vin2_alarm changed to state %d\n", 
                     (p_denverton_data->pwr_good_bits&0x8000? 0: 1) );
        }
#endif
    }

/* Always update threshold alarms ignoring delay time since thresholds can change directly without delay */
/* Temp Alarms post processing */
#if defined(NUM_UCEXT_DENVERTON_TEMPS) && (NUM_UCEXT_DENVERTON_TEMPS>0)
    for(i=0;i<NUM_UCEXT_DENVERTON_TEMPS;i++) {
        
        // Handle Emergency Alarm Independently (ignore hysterisis when invalid values)
        prior_val = p_denverton_data->temp_emerg_alarms[i];
        if(p_denverton_data->temp_emerg_alarms[i] && (p_denverton_data->temp_emerg_hysts[i]<p_denverton_data->temp_emergs[i]) ) {
            if( p_denverton_data->temp_in_vals[i]<p_denverton_data->temp_emerg_hysts[i]) {
                p_denverton_data->temp_emerg_alarms[i] = 0;
            }
        }
        else
        {
            if( p_denverton_data->temp_in_vals[i]>=p_denverton_data->temp_emergs[i]) {
                p_denverton_data->temp_emerg_alarms[i] = 1;
            }
            else {
                p_denverton_data->temp_emerg_alarms[i] = 0;
            }
        }
        if(p_denverton_data->temp_emerg_alarms[i]!=prior_val) {
            dev_info(p_denverton_data->dev_hwmon, "ALERT denverton temp%d_emerg_alarm changed to state %d (temp=%d)\n", 
                     (i+1), p_denverton_data->temp_emerg_alarms[i], p_denverton_data->temp_in_vals[i]/1000);
        }

        // Handle Critical Alarm Independently (ignore hysterisis when invalid values)
        prior_val = p_denverton_data->temp_crit_alarms[i];
        if(p_denverton_data->temp_crit_alarms[i] && (p_denverton_data->temp_crit_hysts[i]<p_denverton_data->temp_crits[i]) ) {
            if( p_denverton_data->temp_in_vals[i]<p_denverton_data->temp_crit_hysts[i]) {
                p_denverton_data->temp_crit_alarms[i] = 0;
            }
        }
        else
        {
            if( p_denverton_data->temp_in_vals[i]>=p_denverton_data->temp_crits[i]) {
                p_denverton_data->temp_crit_alarms[i] = 1;
            }
            else {
                p_denverton_data->temp_crit_alarms[i] = 0;
            }
        }
        if(p_denverton_data->temp_crit_alarms[i]!=prior_val) {
            dev_info(p_denverton_data->dev_hwmon, "ALERT denverton temp%d_crit_alarm changed to state %d (temp=%d)\n", 
                     (i+1), p_denverton_data->temp_crit_alarms[i], p_denverton_data->temp_in_vals[i]/1000);
        }

        // Handle Max Alarm Independently (ignore hysterisis when invalid values)
        prior_val = p_denverton_data->temp_max_alarms[i];
        if(p_denverton_data->temp_max_alarms[i] && (p_denverton_data->temp_max_hysts[i]<p_denverton_data->temp_maxs[i]) ) {
            if( p_denverton_data->temp_in_vals[i]<p_denverton_data->temp_max_hysts[i]) {
                p_denverton_data->temp_max_alarms[i] = 0;
            }
        }
        else
        {
            if( p_denverton_data->temp_in_vals[i]>=p_denverton_data->temp_maxs[i]) {
                p_denverton_data->temp_max_alarms[i] = 1;
            }
            else {
                p_denverton_data->temp_max_alarms[i] = 0;
            }
        }
        if(p_denverton_data->temp_max_alarms[i]!=prior_val) {
            dev_info(p_denverton_data->dev_hwmon, "ALERT denverton temp%d_max_alarm changed to state %d (temp=%d)\n", 
                     (i+1), p_denverton_data->temp_max_alarms[i], p_denverton_data->temp_in_vals[i]/1000);
        }

        // Handle Min Alarm Independently (ignore hysterisis when invalid values)
        prior_val = p_denverton_data->temp_min_alarms[i];
        if(p_denverton_data->temp_min_alarms[i] && (p_denverton_data->temp_min_hysts[i]>p_denverton_data->temp_mins[i]) ) {
            if( p_denverton_data->temp_in_vals[i]>p_denverton_data->temp_min_hysts[i]) {
                p_denverton_data->temp_min_alarms[i] = 0;
            }
        }
        else
        {
            if( p_denverton_data->temp_in_vals[i]<=p_denverton_data->temp_mins[i]) {
                p_denverton_data->temp_min_alarms[i] = 1;
            }
            else {
                p_denverton_data->temp_min_alarms[i] = 0;
            }
        }
        if(p_denverton_data->temp_min_alarms[i]!=prior_val) {
            dev_info(p_denverton_data->dev_hwmon, "ALERT denverton temp%d_min_alarm changed to state %d (temp=%d)\n", 
                     (i+1), p_denverton_data->temp_min_alarms[i], p_denverton_data->temp_in_vals[i]/1000);
        }

    }
#endif

/* Fan alarms post processing */
#if defined(NUM_UCEXT_DENVERTON_FANS) && (NUM_UCEXT_DENVERTON_FANS>0)
    fan_fault = 0;
    fan_alarm = 0;
    actual_fans = NUM_UCEXT_DENVERTON_FANS;
    non_zero_fans = 0;
    fan_avg = 0;

    // Only look at actual fans reported on this hardware rev
    if( (p_denverton_data->chip_num_fans<NUM_UCEXT_DENVERTON_FANS) && (p_denverton_data->chip_num_fans>0) ) {
        actual_fans = p_denverton_data->chip_num_fans;
    }

    // Calculate the average for the non-zero fans to compare to for degradation
    for(i=0;i<actual_fans;i++) {
        if( p_denverton_data->fan_in_vals[i]>0) {
            non_zero_fans++;
            fan_avg += p_denverton_data->fan_in_vals[i]; 
        }
    }

    if(non_zero_fans>0) {
        fan_avg /= non_zero_fans;
    }
    else{
        fan_avg = 0;
    }

    for(i=0;i<NUM_UCEXT_DENVERTON_FANS;i++) {
        // Set the fan alarms using crude method for now (+/- 30% of non-zero avaerage) or test bit.
        prior_val = p_denverton_data->fan_alarms[i];
        if( ((((p_denverton_data->fan_test_alarm_bits)>>i)&1) || (p_denverton_data->fan_in_vals[i]<(fan_avg*7/10)) ||
             (p_denverton_data->fan_in_vals[i]>(fan_avg*13/10))) && (i<actual_fans) ) {
            // track individiual fan alarms
            p_denverton_data->fan_alarms[i] = 1;
            // If any single fan is alarming, system is alarming
            fan_alarm = 1; 
        }
        else {
            p_denverton_data->fan_alarms[i] = 0; 
        }
        if(p_denverton_data->fan_alarms[i]!=prior_val) {
            dev_info(p_denverton_data->dev_hwmon, "ALERT denverton fan%d_alarm changed to state %d (rpm=%d)\n", 
                     (i+1), p_denverton_data->fan_alarms[i], p_denverton_data->fan_in_vals[i]);
        }

        // Set the fan faults using crude method for now of zero or test bit or for new hardware not present.
        prior_val = p_denverton_data->fan_faults[i];
        if( ((((p_denverton_data->fan_test_fault_bits)>>i)&1) || (p_denverton_data->fan_in_vals[i]==0) || (p_denverton_data->fan_present[i]==0) )
            && (i<actual_fans) 
#if defined(NUM_UCEXT_DENVERTON_PWMS) && (NUM_UCEXT_DENVERTON_PWMS>0)
            && (p_denverton_data->pwm_in_vals[0]!=0) 
            && (p_denverton_data->pwm_en_vals[0]!=0)
#endif
          ) {
            // track individiual fan faults
            p_denverton_data->fan_faults[i] = 1;
            // Count the number of fans in fault
            fan_fault++; 
        }
        else {
            p_denverton_data->fan_faults[i] = 0; 
        }
        if(p_denverton_data->fan_faults[i]!=prior_val) {
            dev_info(p_denverton_data->dev_hwmon, "ALERT denverton fan%d_fault changed to state %d (rpm=%d)\n", 
                     (i+1), p_denverton_data->fan_faults[i], p_denverton_data->fan_in_vals[i]);
        }

    }
#endif

#if defined(UCEXT_DENVERTON_SYSTEM_FAN) && (UCEXT_DENVERTON_SYSTEM_FAN!=0)
    // Store the system fan alarm (any single fan alarming) and system fan fault (all fans faulting) status
    prior_val = p_denverton_data->system_fan_alarm;
    p_denverton_data->system_fan_alarm = fan_alarm;
    p_denverton_data->system_fan_avg = fan_avg; 
    if(p_denverton_data->system_fan_alarm!=prior_val) {
        dev_info(p_denverton_data->dev_hwmon, "ALERT denverton system fan_alarm changed to state %d (avg rpm=%d)\n", 
                 p_denverton_data->system_fan_alarm, p_denverton_data->system_fan_avg);
    }

    prior_val = p_denverton_data->system_fan_fault;
    if(fan_fault>=actual_fans) {
        p_denverton_data->system_fan_fault = 1;
    }
    else {
        p_denverton_data->system_fan_fault = 0;
    }
    if(p_denverton_data->system_fan_fault!=prior_val) {
        dev_info(p_denverton_data->dev_hwmon, "ALERT denverton system fan_fault changed to state %d (avg rpm=%d)\n", 
                 p_denverton_data->system_fan_fault, p_denverton_data->system_fan_avg);
    }
#endif

/* Volt Alarms post processing */
#if defined(NUM_UCEXT_DENVERTON_VOLTS) && (NUM_UCEXT_DENVERTON_VOLTS>0)
    for(i=0;i<NUM_UCEXT_DENVERTON_VOLTS;i++) {

        prior_val = p_denverton_data->volt_max_alarms[i];
        if( p_denverton_data->volt_in_vals[i]>p_denverton_data->volt_maxs[i]) {
            p_denverton_data->volt_max_alarms[i] = 1;
        }
        else {
            p_denverton_data->volt_max_alarms[i] = 0;
        }
        if(p_denverton_data->volt_max_alarms[i]!=prior_val) {
            dev_info(p_denverton_data->dev_hwmon, "ALERT denverton in%d_max_alarm changed to state %d (mV=%d)\n", 
                     i, p_denverton_data->volt_max_alarms[i], p_denverton_data->volt_in_vals[i]);
        }

        prior_val = p_denverton_data->volt_min_alarms[i];
        if( p_denverton_data->volt_in_vals[i]<p_denverton_data->volt_mins[i]) {
            p_denverton_data->volt_min_alarms[i] = 1;
        }
        else {
            p_denverton_data->volt_min_alarms[i] = 0;
        }
        if(p_denverton_data->volt_min_alarms[i]!=prior_val) {
            dev_info(p_denverton_data->dev_hwmon, "ALERT denverton in%d_min_alarm changed to state %d (mV=%d)\n", 
                     i, p_denverton_data->volt_min_alarms[i], p_denverton_data->volt_in_vals[i]);
        }

        prior_val = p_denverton_data->volt_lcrit_alarms[i];
        if( (p_denverton_data->volt_in_vals[i]<p_denverton_data->volt_lcrits[i]) ||
            (!((p_denverton_data->pwr_good_bits>>i)&1)) ) {
            p_denverton_data->volt_lcrit_alarms[i] = 1;

        }
        else
        {
            p_denverton_data->volt_lcrit_alarms[i] = 0;
        }
        if(p_denverton_data->volt_lcrit_alarms[i]!=prior_val) {
            dev_info(p_denverton_data->dev_hwmon, "ALERT denverton in%d_lcrit_alarm changed to state %d (mV=%d, pwrgood=0x%04X)\n", 
                     i, p_denverton_data->volt_lcrit_alarms[i], p_denverton_data->volt_in_vals[i], p_denverton_data->pwr_good_bits&0x0000ffff);
        }
    }
#endif

    error:
    mutex_unlock(&p_denverton_data->update_lock);
    return retval;
}





