#ifndef _UCEXT_PROTOCOL_STRUCTS_H_
#define _UCEXT_PROTOCOL_STRUCTS_H_

#ifndef UCEXT_MAX_MESSAGE_SIZE 
    #define UCEXT_MAX_MESSAGE_SIZE  128
#endif

/*----------------------------------------------------------------------------------------------*/

enum ucext_cmd_enums {
    UCMD_SET_RESET     = 0x25,
    UCMD_GET_VERSION   = 0x26,
    UCMD_SET_PWM       = 0x27,
    UCMD_GET_BUTTON    = 0x2D,
    UCMD_GET_MSP430    = 0x2E,
    UCMD_SET_LEDS      = 0x2F,
    UCMD_GET_HWID      = 0x30,
    UCMD_CLR_BUTTON    = 0x31,
    UCMD_CLR_PWRGOOD   = 0x36,
    UCMD_GET_RESET_CAUSE = 0x37,
    UCMD_SET_WATCHDOG  = 0x38,
    UCMD_GET_CONFIG    = 0x40,
    UCMD_UPDATE_TEMPS  = 0x41,
    UCMD_UPDATE_FANS   = 0x43,
    UCMD_GET_STATUS    = 0x44,
    UCMD_GET_LEDS      = 0x45,
    UCMD_SET_PROTOCOL  = 0x48,
} __attribute__((packed));

typedef enum ucext_cmd_enums        t_ucext_cmd_enum;


enum ucext_result_enums {
    URSP_ACK           = 0xcc,
    URSP_NAK           = 0x33
} __attribute__((packed)) ;

typedef enum ucext_result_enums     t_ucext_result_enum;


typedef struct {
    __u8  len;
    __u8  chksum;
    t_ucext_cmd_enum    cmd;
} __attribute__((packed)) t_ucext_cmd_hdr;


typedef struct {
    t_ucext_cmd_hdr     cmd_hdr;
    __u8                cmd_data[UCEXT_MAX_MESSAGE_SIZE-sizeof(t_ucext_cmd_hdr)];
} __attribute__((packed)) t_ucext_command;


typedef struct {
    t_ucext_result_enum result;
    __u8                len;
    __u8                chksum;
} __attribute__((packed)) t_ucext_rsp_hdr;


typedef struct {
    t_ucext_rsp_hdr     rsp_hdr;
    __u8                rsp_data[UCEXT_MAX_MESSAGE_SIZE-sizeof(t_ucext_rsp_hdr)];
} __attribute__((packed)) t_ucext_response;


typedef union {
    __u8                    raw_msg[UCEXT_MAX_MESSAGE_SIZE];
    t_ucext_command         cmd_msg;
    t_ucext_response        rsp_msg;
}__attribute__((packed)) t_ucext_message;

/*----------------------------------------------------------------------------------------------*/

enum ucext_sensor_enums {
    USENSOR_TYPE_DEVICE     = 0x00,
    USENSOR_TYPE_CHIP       = 0x01,
    USENSOR_TYPE_TEMP       = 0x02,
    USENSOR_TYPE_VOLT       = 0x03,
    USENSOR_TYPE_CURR       = 0x04,
    USENSOR_TYPE_POWER      = 0x05,
    USENSOR_TYPE_ENERGY     = 0x06,
    USENSOR_TYPE_HUMUDITY   = 0x07,
    USENSOR_TYPE_FAN        = 0x08,
    USENSOR_TYPE_PWM        = 0x09,
    USENSOR_TYPE_MISC       = 0x0A,
    USENSOR_TYPE_LAST       = 0x0B
} __attribute__((packed));

typedef enum ucext_sensor_enums     t_ucext_sensor_enum;

/* Not this still needs work, Just a place holder */
typedef struct {
    t_ucext_sensor_enum  sensor;
    __u8                 bytesper;
    __u16                numbytes;
} __attribute__((packed)) t_ucext_sensor_tlv;


#endif

