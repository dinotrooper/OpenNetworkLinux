#ifndef _UCEXT_MSG_H_
#define _UCEXT_MSG_H_

#ifndef UCEXT_MSG_STUB
    #define UCEXT_MSG_STUB  1 
#endif

#ifndef UCEXT_MSG_MAX_SIZE
    #define UCEXT_MSG_MAX_SIZE  256
#endif

#ifndef UCEXT_MSG_TIMEOUT_MSEC
    #define UCEXT_MSG_TIMEOUT_MSEC  2000
#endif

#ifndef UCEXT_MSG_MAX_PROTOCOL
    #define UCEXT_MSG_MAX_PROTOCOL  255
#endif

#ifndef UCEXT_MSG_MIN_PROTOCOL
    #define UCEXT_MSG_MIN_PROTOCOL 0
#endif

#ifndef UCEXT_MSG_BIN_REVISION_EXPECTED
    #define UCEXT_MSG_BIN_REVISION_EXPECTED 0x000000
#endif

extern void *ucext_msg_init(struct device *dev);
extern void ucext_msg_exit(struct device *dev);
extern int ucext_msg_restricted(void *p_data);

extern int ucext_msg_update_config(int devid, void *p_data, int *p_cfg_vals);
extern int ucext_msg_update_temps(int devid, void *p_data, int *p_temp_vals);
extern int ucext_msg_update_volts(int devid, void *p_data, int *p_volt_vals, unsigned int *pwr_good, unsigned int pwr_test);
extern int ucext_msg_update_currs(int devid, void *p_data, int *p_curr_vals);
extern int ucext_msg_update_fans(int devid, int protocol, int cfg_fans, void *p_data, int *p_fan_vals, int *p_pres_vals);
extern int ucext_msg_update_buttons(int devid, void *p_data, int *p_button_vals);
extern int ucext_msg_update_aux_status(int devid, void *p_data, int *p_status_vals);
extern int ucext_msg_update_leds(int devid, int protocol, int cfg_leds, void *p_data, int *p_color_vals, int *p_rate_vals);

extern int ucext_msg_set_pwms(int devid, void *p_data, int *p_pwmen_vals, int *p_pwm_vals );
extern int ucext_msg_set_leds(int devid, void *p_data, int *p_color_vals, int *p_rate_vals );
extern int ucext_msg_clear_button(int devid, void *p_data, int button );

extern int ucext_msg_set_reboot(int devid, void *p_data, int *p_reboot_val);
extern int ucext_msg_set_watchdog(int devid, void *p_data, int *p_watchdog_val);
extern int ucext_msg_clear_pwrgood(int devid, void *p_data);

extern int ucext_msg_set_msg_protocol(int devid, void *p_data, int protocol);

#endif

