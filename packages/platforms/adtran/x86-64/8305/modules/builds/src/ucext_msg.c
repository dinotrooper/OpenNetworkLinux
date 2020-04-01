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

#include "ucext_protocol_structs.h"
#include "ucext_dev_tivac.h"
#include "ucext_dev_MSP430sharedpwr.h"
#include "ucext_dev_MSP430tomahawk.h"
#include "ucext_dev_MSP430denverton.h"
#include "ucext_msg.h"
#include "ucext_usb.h"

struct ucext_msg_data {
    struct device *dev;
    struct mutex  update_lock;
    unsigned long last_rcvd_jiffies;      /* In jiffies */
    unsigned long last_sent_jiffies;      /* In jiffies */
    char valid;                   /* !=0 if following fields are valid */
    char msg_restrict;            /* !=0 when messages restricted      */

//    unsigned char tx_buff[UCEXT_MSG_MAX_SIZE];
//    unsigned char rx_buff[UCEXT_MSG_MAX_SIZE];

    int timeout_msec;
    t_ucext_message   tx_buff;
    t_ucext_message   rx_buff;
    int tx_cnt, tx_wrote_cnt;
    int rx_cnt;

    __u64             msgs_sent;
    __u64             msgs_rcvd;
};


void *ucext_msg_init(struct device *dev)
{
    struct ucext_msg_data *p_msg_data;

    if(dev==NULL) {
        dev_err(dev, "ucext_msg_init NULL parent device pointer\n");
        return 0;
    }

#if (!defined(UCEXT_TIVAC_DEVM_MEM_ALLOC) || (UCEXT_TIVAC_DEVM_MEM_ALLOC==0))
    p_msg_data = kzalloc(sizeof(struct ucext_msg_data), GFP_KERNEL);
#else
    p_msg_data = devm_kzalloc(dev, sizeof(struct ucext_msg_data), GFP_KERNEL);
#endif

    if(!p_msg_data) {
        dev_err(dev, "ucext_msg_init failed to allocate private message data\n");
        return 0;
    }

    /* Now initialize the msg subsystem private private data */
    p_msg_data->dev = dev;

    mutex_init(&p_msg_data->update_lock);

    p_msg_data->timeout_msec = UCEXT_MSG_TIMEOUT_MSEC;

    p_msg_data->msgs_rcvd = 0;
    p_msg_data->last_rcvd_jiffies = jiffies;

    p_msg_data->msgs_sent = 0;
    p_msg_data->last_sent_jiffies = jiffies;

    p_msg_data->msg_restrict = 1;
    p_msg_data->valid = 1;

    return p_msg_data;
}


void ucext_msg_exit(struct device *dev) {
    struct ucext_msg_data *p_msg_data;

    if(dev==NULL) {
        return;
    }

    p_msg_data = ucext_get_dev_msg_data(dev);

    if(p_msg_data==NULL) {
        return;
    }

    p_msg_data->valid = 0;
    mutex_destroy(&p_msg_data->update_lock);
#if (!defined(UCEXT_TIVAC_DEVM_MEM_ALLOC) || (UCEXT_TIVAC_DEVM_MEM_ALLOC==0))
    kfree (p_msg_data);
#endif
}

#ifndef UCEXT_MSG_STUB
/* Coversion helper function for the MSP430 Currents and Temps */
static long msp430_linear11_to_milli(__s16 l)
{
        s16 exponent;
        s32 mantissa;
        long val;

        exponent = l >> 11;
        mantissa = ((s16)((l & 0x7ff) << 5)) >> 5;

        val = mantissa;

        /* scale result to milli-units */
        val = val * 1000L;

        if (exponent >= 0)
                val <<= exponent;
        else
                val >>= -exponent;

        return val;
}


/* Coversion helper function for the MSP430 Voltages */
static long msp430_volts_milli(__s16 l)
{
        long val;
        val = l * 1953L;
        val = val / 1000L;
        return val;
}
#endif


int ucext_msg_restricted(void *p_data) {
    struct ucext_msg_data *p_msg_data = p_data;

    if(!p_msg_data || !p_msg_data->valid || (p_msg_data->msg_restrict!=0) ) {
        return 1;
    }

    return 0;
}

int ucext_msg_update_config(int devid, void *p_data, int *p_cfg_vals) {
    int retval = 0;
    struct ucext_msg_data *p_msg_data = p_data;
    int *p_vals = p_cfg_vals;
    unsigned long fw_version;

    if(!p_msg_data || !p_cfg_vals) {
        dev_err(p_msg_data->dev, "ucext_msg_update_config null pointer to private data\n");
        return -EINVAL;
    }

    if(!p_msg_data->valid) {
        dev_err(p_msg_data->dev, "ucext_msg_update_config private data not valid\n");
        return -ENODEV;
    }

// LTG Temporarily leave all messages disabled until Tiva supports it!  
//  if(p_msg_data->msg_restrict!=0) {
//      return 0;
//  }

    mutex_lock(&p_msg_data->update_lock);
    switch(devid) {
        case UCEXT_TIVAC_DEVICE_INDEX:

            // Use the get version command to get the clock chips versions
            p_msg_data->tx_buff.cmd_msg.cmd_hdr.len = sizeof(t_ucext_cmd_hdr);
            p_msg_data->tx_buff.cmd_msg.cmd_hdr.chksum = UCMD_GET_VERSION;
            p_msg_data->tx_buff.cmd_msg.cmd_hdr.cmd =  UCMD_GET_VERSION;
            p_msg_data->tx_cnt = sizeof(t_ucext_cmd_hdr);

            memset((void *)&p_msg_data->rx_buff,0,sizeof(t_ucext_response));  
            p_msg_data->rx_cnt = 0;
            retval = ucext_cmd_response(p_msg_data->dev, p_msg_data->tx_cnt, (unsigned char *)&p_msg_data->tx_buff, 
                                        (unsigned char *)&p_msg_data->rx_buff, &p_msg_data->rx_cnt, p_msg_data->timeout_msec);

            if(retval!=0) {
                dev_info(p_msg_data->dev, "ucext_msg_update_config cmd getversion response failure\n");
                retval = -EIO;
                goto error;
            }

            // On NAK responses to version message, let's just not message anymore till clears up
            if( p_msg_data->rx_buff.rsp_msg.rsp_hdr.result!=URSP_ACK ) {
                if(p_msg_data->msg_restrict!=0) {
                    dev_info(p_msg_data->dev, "ucext_msg_update_config cmd getversion response NAK restricting messages\n");
                    p_msg_data->msg_restrict=1;
                }
                retval = 0;
                break;
            }

            p_vals[0] = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[0];  // FW Major
            p_vals[1] = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[1];  // FW Minor
            p_vals[2] = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[2];  // Boot Major
            p_vals[3] = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[3];  // Boot Minor
            p_vals[4] = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[4];  // Msg Protocol

            p_vals[11] = (int) (p_msg_data->rx_buff.rsp_msg.rsp_data[16]>>4);    // ClkGen Major Rev
            p_vals[12] = (int) (p_msg_data->rx_buff.rsp_msg.rsp_data[16]&0x0F);  // ClkGen Minor Rev

            if(p_msg_data->msg_restrict!=0) {
                fw_version = ( (((unsigned int)p_msg_data->rx_buff.rsp_msg.rsp_data[0])<<16) | 
                                ((unsigned int)p_msg_data->rx_buff.rsp_msg.rsp_data[1]) );
                if(fw_version>=UCEXT_MSG_BIN_REVISION_EXPECTED)
                {
// LTG Temporary block of relesaing restrictions.
                    dev_info(p_msg_data->dev, "ucext_msg_update_config (detected version %lx) message restrictions removed\n", fw_version); 
                    p_msg_data->msg_restrict=0;
                }
            }

//            dev_info(p_msg_data->dev, "ucext_msg_update_config cmd getversion clk1=%d.%d\n", 
//                     p_cfg_vals[10],p_cfg_vals[11]);

            p_msg_data->tx_buff.cmd_msg.cmd_hdr.len = sizeof(t_ucext_cmd_hdr);
            p_msg_data->tx_buff.cmd_msg.cmd_hdr.chksum = UCMD_GET_CONFIG;
            p_msg_data->tx_buff.cmd_msg.cmd_hdr.cmd =  UCMD_GET_CONFIG;
            p_msg_data->tx_cnt = sizeof(t_ucext_cmd_hdr);

            memset((void *)&p_msg_data->rx_buff,0,sizeof(t_ucext_response));  
            p_msg_data->rx_cnt = 0;
            retval = ucext_cmd_response(p_msg_data->dev, p_msg_data->tx_cnt, (unsigned char *)&p_msg_data->tx_buff, 
                                        (unsigned char *)&p_msg_data->rx_buff, &p_msg_data->rx_cnt, p_msg_data->timeout_msec );

            if(retval!=0) {
                dev_info(p_msg_data->dev, "ucext_msg_update_config cmd response failure\n");
                retval = -EIO;
                goto error;
            }

            if( (p_msg_data->rx_buff.rsp_msg.rsp_hdr.result!=URSP_ACK) ||
                ((p_msg_data->rx_buff.rsp_msg.rsp_hdr.len<((sizeof(t_ucext_rsp_hdr)-1)+10))) ) {
                if(p_msg_data->msg_restrict==0) {
                    dev_info(p_msg_data->dev, "ucext_msg_update_config cmd getconfig response header invalid\n");
                }
                retval = 0;
                break;
            }

            p_vals[5] = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[5];  // HW ID
            p_vals[6] = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[6];  // Num PWMs
            p_vals[7] = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[7];  // Num Fans
            p_vals[8] = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[8];  // Num temps
            p_vals[9] = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[9];  // Num Auxilary Devices
            p_vals[10] = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[10]; // Num LEDs supported 

#ifndef UCEXT_MSG_STUB
            // Use the get version command to get the clock chips versions
            p_msg_data->tx_buff.cmd_msg.cmd_hdr.len = sizeof(t_ucext_cmd_hdr);
            p_msg_data->tx_buff.cmd_msg.cmd_hdr.chksum = UCMD_GET_RESET_CAUSE;
            p_msg_data->tx_buff.cmd_msg.cmd_hdr.cmd =  UCMD_GET_RESET_CAUSE;
            p_msg_data->tx_cnt = sizeof(t_ucext_cmd_hdr);

            memset((void *)&p_msg_data->rx_buff,0,sizeof(t_ucext_response));  
            p_msg_data->rx_cnt = 0;
            retval = ucext_cmd_response(p_msg_data->dev, p_msg_data->tx_cnt, (unsigned char *)&p_msg_data->tx_buff, 
                                        (unsigned char *)&p_msg_data->rx_buff, &p_msg_data->rx_cnt, p_msg_data->timeout_msec);

            if(retval!=0) {
                dev_info(p_msg_data->dev, "ucext_msg_update_config cmd getresetcause response failure\n");
                retval = -EIO;
                goto error;
            }

            if( (p_msg_data->rx_buff.rsp_msg.rsp_hdr.result!=URSP_ACK) ||
                ((p_msg_data->rx_buff.rsp_msg.rsp_hdr.len<((sizeof(t_ucext_rsp_hdr)-1)+1))) ) {
                dev_info(p_msg_data->dev, "ucext_msg_update_config cmd getresetcause response header invalid\n");
                retval = -EIO;
                goto error;
            }

            p_vals[13] = (int) (p_msg_data->rx_buff.rsp_msg.rsp_data[0]);        // Reset Cause Value
#endif
            break;

        case UCEXT_SHAREDPWR_DEVICE_INDEX:
        case UCEXT_TOMAHAWK_DEVICE_INDEX:
        case UCEXT_DENVERTON_DEVICE_INDEX:
            p_msg_data->tx_buff.cmd_msg.cmd_hdr.len = sizeof(t_ucext_cmd_hdr);
            p_msg_data->tx_buff.cmd_msg.cmd_hdr.chksum = UCMD_GET_VERSION;
            p_msg_data->tx_buff.cmd_msg.cmd_hdr.cmd =  UCMD_GET_VERSION;
            p_msg_data->tx_cnt = sizeof(t_ucext_cmd_hdr);

            memset((void *)&p_msg_data->rx_buff,0,sizeof(t_ucext_response));  
            p_msg_data->rx_cnt = 0;
            retval = ucext_cmd_response(p_msg_data->dev, p_msg_data->tx_cnt, (unsigned char *)&p_msg_data->tx_buff, 
                                        (unsigned char *)&p_msg_data->rx_buff, &p_msg_data->rx_cnt, p_msg_data->timeout_msec);

            if(retval!=0) {
                dev_info(p_msg_data->dev, "ucext_msg_update_config cmd getversion response failure\n");
                retval = -EIO;
                goto error;
            }

            if( p_msg_data->rx_buff.rsp_msg.rsp_hdr.result!=URSP_ACK ) {
                if(p_msg_data->msg_restrict!=0) {
                    dev_info(p_msg_data->dev, "ucext_msg_update_config cmd getversion response NAK restricting messages\n");
                    p_msg_data->msg_restrict=1;
                }
                retval = 0;
                break;
            }

            p_vals[2] = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[17];     // Protocol Version

            if(devid==UCEXT_SHAREDPWR_DEVICE_INDEX) {
                p_vals[0] = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[4];  // VERSION Major
                p_vals[1] = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[5];  // VERSION Minor
            }
            else if(devid==UCEXT_TOMAHAWK_DEVICE_INDEX) {
                p_vals[0] = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[6];  // VERSION Major
                p_vals[1] = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[7];  // VERSION Minor
            }
            else {
                p_vals[0] = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[8];  // VERSION Major
                p_vals[1] = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[9];  // VERSION Minor
            }
            // dev_info(p_msg_data->dev, "ucext_msg_update_config[dev=%d] cmd getversion=%d.%d\n", devid, p_vals[0], p_vals[1]);

            if(p_msg_data->msg_restrict!=0) {
                fw_version = ( (((unsigned int)p_msg_data->rx_buff.rsp_msg.rsp_data[0])<<16) | 
                                ((unsigned int)p_msg_data->rx_buff.rsp_msg.rsp_data[1]) );
                if(fw_version>=UCEXT_MSG_BIN_REVISION_EXPECTED) {
// LTG Temporary block of relesaing restrictions.
                  dev_info(p_msg_data->dev, "ucext_msg_update_config message restriction lifted\n");
                  p_msg_data->msg_restrict=0;
                }
                else {
                    break;
                }
            }

            p_msg_data->tx_buff.cmd_msg.cmd_hdr.len = sizeof(t_ucext_cmd_hdr);
            p_msg_data->tx_buff.cmd_msg.cmd_hdr.chksum = UCMD_GET_HWID;
            p_msg_data->tx_buff.cmd_msg.cmd_hdr.cmd =  UCMD_GET_HWID;
            p_msg_data->tx_cnt = sizeof(t_ucext_cmd_hdr);

            memset((void *)&p_msg_data->rx_buff,0,sizeof(t_ucext_response));  
            p_msg_data->rx_cnt = 0;
            retval = ucext_cmd_response(p_msg_data->dev, p_msg_data->tx_cnt, (unsigned char *)&p_msg_data->tx_buff, 
                                        (unsigned char *)&p_msg_data->rx_buff, &p_msg_data->rx_cnt, p_msg_data->timeout_msec);

            if(retval!=0) {
                dev_info(p_msg_data->dev, "ucext_msg_update_config cmd get HWID response failure\n");
                retval = -EIO;
                goto error;
            }

            if( (p_msg_data->rx_buff.rsp_msg.rsp_hdr.result!=URSP_ACK) ||
                ((p_msg_data->rx_buff.rsp_msg.rsp_hdr.len<((sizeof(t_ucext_rsp_hdr)-1)+1))) ) {
                dev_info(p_msg_data->dev, "ucext_msg_update_config cmd get HWID response header invalid\n");
                retval = -EIO;
                goto error;
            }

            p_vals[3] = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[0];  // HW ID
            // dev_info(p_msg_data->dev, "ucext_msg_update_config[dev=%d] cmd get HWID =%d\n", devid, p_vals[2]);

            break;

        default:
            retval = 0;
            break;
    }
    error:
    mutex_unlock(&p_msg_data->update_lock);
    return retval;
}


int ucext_msg_update_temps(int devid, void *p_data, int *p_temp_vals) {
    int retval = 0;
    struct ucext_msg_data *p_msg_data = p_data;
    int i;
    __s16 *pregs;


    if(!p_msg_data || !p_temp_vals) {
        dev_err(p_msg_data->dev, "ucext_msg_update_temps[%d] null pointer to private data\n", devid);
        return -EINVAL;
    }

    if(!p_msg_data->valid) {
        dev_err(p_msg_data->dev, "ucext_msg_update_temps[%d] private data not valid\n", devid);
        return -ENODEV;
    }

// LTG Temporary block of temps
    if(p_msg_data->msg_restrict!=0 || p_msg_data->msg_restrict==0) {
        return 0;
    }

    mutex_lock(&p_msg_data->update_lock);

    switch(devid) {
        /* Update Temps for TIVAC-C device */
        case UCEXT_TIVAC_DEVICE_INDEX:
            p_msg_data->tx_buff.cmd_msg.cmd_hdr.len = sizeof(t_ucext_cmd_hdr);
            p_msg_data->tx_buff.cmd_msg.cmd_hdr.chksum = UCMD_UPDATE_TEMPS;
            p_msg_data->tx_buff.cmd_msg.cmd_hdr.cmd =  UCMD_UPDATE_TEMPS;
            p_msg_data->tx_cnt = p_msg_data->tx_buff.cmd_msg.cmd_hdr.len;

            memset((void *)&p_msg_data->rx_buff,0,sizeof(t_ucext_response));  
            p_msg_data->rx_cnt = 0;
            retval = ucext_cmd_response(p_msg_data->dev, p_msg_data->tx_cnt, (unsigned char *)&p_msg_data->tx_buff, 
                                        (unsigned char *)&p_msg_data->rx_buff, &p_msg_data->rx_cnt, p_msg_data->timeout_msec);

            if(retval!=0) {
                dev_err(p_msg_data->dev, "ucext_msg_update_temps[%d] cmd response failure\n", devid);
                goto error;
            }

            if( (p_msg_data->rx_buff.rsp_msg.rsp_hdr.result != URSP_ACK) ||  
                (p_msg_data->rx_buff.rsp_msg.rsp_hdr.len!=((sizeof(t_ucext_rsp_hdr)-1)+(2*NUM_UCEXT_TIVAC_TEMPS))) ) {
                dev_info(p_msg_data->dev, "ucext_msg_update_temps[%d] response header (0x%02x,0x%02x) invalid\n", devid,
                         p_msg_data->rx_buff.rsp_msg.rsp_hdr.result, p_msg_data->rx_buff.rsp_msg.rsp_hdr.len);
                retval = -EIO;
                goto error;
            }

            pregs = (__s16 *)p_msg_data->rx_buff.rsp_msg.rsp_data;
            for(i=0; i<NUM_UCEXT_TIVAC_TEMPS; i++) {
                p_temp_vals[i] = ( ((*pregs++)&~0x000f) * 1000 + 128) / 256;  
            }
            break;

#ifndef UCEXT_MSG_STUB
        /* Update Temps for MSP430 Tomahawk device */
        case UCEXT_TOMAHAWK_DEVICE_INDEX:
            p_msg_data->tx_buff.cmd_msg.cmd_hdr.len = sizeof(t_ucext_cmd_hdr) + 1;
            p_msg_data->tx_buff.cmd_msg.cmd_hdr.chksum = UCMD_GET_MSP430 + 1;
            p_msg_data->tx_buff.cmd_msg.cmd_hdr.cmd =  UCMD_GET_MSP430;
            p_msg_data->tx_buff.cmd_msg.cmd_data[0] =  (1);
            p_msg_data->tx_cnt = p_msg_data->tx_buff.cmd_msg.cmd_hdr.len;

            memset((void *)&p_msg_data->rx_buff,0,sizeof(t_ucext_response));  
            p_msg_data->rx_cnt = 0;
            retval = ucext_cmd_response(p_msg_data->dev, p_msg_data->tx_cnt, (unsigned char *)&p_msg_data->tx_buff, 
                                        (unsigned char *)&p_msg_data->rx_buff, &p_msg_data->rx_cnt, p_msg_data->timeout_msec);

            if(retval!=0) {
                dev_info(p_msg_data->dev, "ucext_msg_update_temps[%d] cmd response failure! cmd=(0x%02x,0x%02x 0x%02x 0x%02x) invalid\n", devid,
                        p_msg_data->tx_buff.cmd_msg.cmd_hdr.len, p_msg_data->tx_buff.cmd_msg.cmd_hdr.chksum,
                        p_msg_data->tx_buff.cmd_msg.cmd_hdr.cmd, p_msg_data->tx_buff.cmd_msg.cmd_data[0]);
                goto error;
            }

            // Check for ACK, length, and num temps
            if( (p_msg_data->rx_buff.rsp_msg.rsp_hdr.result != URSP_ACK) ||
                (p_msg_data->rx_buff.rsp_msg.rsp_hdr.len != 25) || 
                (p_msg_data->rx_buff.rsp_msg.rsp_data[2] != 1)) {
                dev_err(p_msg_data->dev, "ucext_msg_update_temps[%d] response header (0x%02x,0x%02x) invalid\n", devid,
                         p_msg_data->rx_buff.rsp_msg.rsp_hdr.result, p_msg_data->rx_buff.rsp_msg.rsp_hdr.len);
                retval = -EIO;
                goto error;
            }

            pregs = (__s16 *)(&p_msg_data->rx_buff.rsp_msg.rsp_data[19]);
            for(i=0; i<NUM_UCEXT_TOMAHAWK_TEMPS; i++) {
                p_temp_vals[i] = msp430_linear11_to_milli(*pregs++);
            }
            break;
#endif

        default:
            retval = 0;
            break;
    }
    error:
    mutex_unlock(&p_msg_data->update_lock);
    return retval;
}


int ucext_msg_update_currs(int devid, void *p_data, int *p_curr_vals) {
    int retval = 0;
    struct ucext_msg_data *p_msg_data = p_data;

    if(!p_msg_data || !p_curr_vals) {
        dev_err(p_msg_data->dev, "ucext_msg_update_currs[%d] null pointer to private data\n", devid);
        return -EINVAL;
    }

    if(!p_msg_data->valid) {
        dev_err(p_msg_data->dev, "ucext_msg_update_currs[%d] private data not valid\n", devid);
        return -ENODEV;
    }

    if(p_msg_data->msg_restrict!=0) {
        return 0;
    }

#ifndef UCEXT_MSG_STUB
    int i;
    __s16 *pregs;

    mutex_lock(&p_msg_data->update_lock);

    switch(devid) {

        /* Update Currents for MSP430 Tomahawk device */
        case UCEXT_TOMAHAWK_DEVICE_INDEX:
            p_msg_data->tx_buff.cmd_msg.cmd_hdr.len = sizeof(t_ucext_cmd_hdr) + 1;
            p_msg_data->tx_buff.cmd_msg.cmd_hdr.chksum = UCMD_GET_MSP430 + 1;
            p_msg_data->tx_buff.cmd_msg.cmd_hdr.cmd =  UCMD_GET_MSP430;
            p_msg_data->tx_buff.cmd_msg.cmd_data[0] =  (1);
            p_msg_data->tx_cnt = p_msg_data->tx_buff.cmd_msg.cmd_hdr.len;

            memset((void *)&p_msg_data->rx_buff,0,sizeof(t_ucext_response));  
            p_msg_data->rx_cnt = 0;
            retval = ucext_cmd_response(p_msg_data->dev, p_msg_data->tx_cnt, (unsigned char *)&p_msg_data->tx_buff, 
                                        (unsigned char *)&p_msg_data->rx_buff, &p_msg_data->rx_cnt, p_msg_data->timeout_msec);

            if(retval!=0) {
                dev_info(p_msg_data->dev, "ucext_msg_update_currs[%d] cmd response failure! cmd=(0x%02x,0x%02x 0x%02x 0x%02x) invalid\n", devid,
                        p_msg_data->tx_buff.cmd_msg.cmd_hdr.len, p_msg_data->tx_buff.cmd_msg.cmd_hdr.chksum,
                        p_msg_data->tx_buff.cmd_msg.cmd_hdr.cmd, p_msg_data->tx_buff.cmd_msg.cmd_data[0]);
                goto error;
            }

            // Check for ACK, length, and num currents
            if( (p_msg_data->rx_buff.rsp_msg.rsp_hdr.result != URSP_ACK) ||
                (p_msg_data->rx_buff.rsp_msg.rsp_hdr.len != 25) || 
                (p_msg_data->rx_buff.rsp_msg.rsp_data[1] != 1)) {
                dev_err(p_msg_data->dev, "ucext_msg_update_currs[%d] response header (0x%02x,0x%02x) invalid\n", devid,
                         p_msg_data->rx_buff.rsp_msg.rsp_hdr.result, p_msg_data->rx_buff.rsp_msg.rsp_hdr.len);
                retval = -EIO;
                goto error;
            }

            pregs = (__s16 *)(&p_msg_data->rx_buff.rsp_msg.rsp_data[17]);
            for(i=0; i<NUM_UCEXT_TOMAHAWK_CURRS; i++) {
                p_curr_vals[i] = msp430_linear11_to_milli(*pregs++);
            }
            break;

        default:
            retval = 0;
            break;
    }
    error:
    mutex_unlock(&p_msg_data->update_lock);
#endif
    return retval;
}


int ucext_msg_update_volts(int devid, void *p_data, int *p_volt_vals, unsigned int *pwr_good, unsigned int pwr_test) {
    int retval = 0;
    struct ucext_msg_data *p_msg_data = p_data;

    if(!p_msg_data || !pwr_good) {
        dev_err(p_msg_data->dev, "ucext_msg_update_volts[%d] null pointer to private data\n", devid);
        return -EINVAL;
    }

    if(!p_msg_data->valid) {
        dev_err(p_msg_data->dev, "ucext_msg_update_volts[%d] private data not valid\n", devid);
        return -ENODEV;
    }

    if(p_msg_data->msg_restrict!=0) {
        return 0;
    }

#ifndef UCEXT_MSG_STUB
    int i;
    __s16 *pregs;

    mutex_lock(&p_msg_data->update_lock);

    switch(devid) {
        /* Update Volts for MSP430 Tomahawk device */
        case UCEXT_TOMAHAWK_DEVICE_INDEX:
            p_msg_data->tx_buff.cmd_msg.cmd_hdr.len = sizeof(t_ucext_cmd_hdr) + 1;
            p_msg_data->tx_buff.cmd_msg.cmd_hdr.chksum = UCMD_GET_MSP430 + 1;
            p_msg_data->tx_buff.cmd_msg.cmd_hdr.cmd =  UCMD_GET_MSP430;
            p_msg_data->tx_buff.cmd_msg.cmd_data[0] =  (1);
            p_msg_data->tx_cnt = p_msg_data->tx_buff.cmd_msg.cmd_hdr.len;

            memset((void *)&p_msg_data->rx_buff,0,sizeof(t_ucext_response));  
            p_msg_data->rx_cnt = 0;
            retval = ucext_cmd_response(p_msg_data->dev, p_msg_data->tx_cnt, (unsigned char *)&p_msg_data->tx_buff, 
                                        (unsigned char *)&p_msg_data->rx_buff, &p_msg_data->rx_cnt, p_msg_data->timeout_msec);

            if(retval!=0) {
                dev_info(p_msg_data->dev, "ucext_msg_update_volts[%d] cmd response failure! cmd=(0x%02x,0x%02x 0x%02x 0x%02x) invalid\n", devid,
                        p_msg_data->tx_buff.cmd_msg.cmd_hdr.len, p_msg_data->tx_buff.cmd_msg.cmd_hdr.chksum,
                        p_msg_data->tx_buff.cmd_msg.cmd_hdr.cmd, p_msg_data->tx_buff.cmd_msg.cmd_data[0]);
                goto error;
            }

            // Check for ACK, length, and num voltages
            if( (p_msg_data->rx_buff.rsp_msg.rsp_hdr.result != URSP_ACK) ||
                (p_msg_data->rx_buff.rsp_msg.rsp_hdr.len != 25) || 
                (p_msg_data->rx_buff.rsp_msg.rsp_data[0] != 6) ||
                (p_msg_data->rx_buff.rsp_msg.rsp_data[3] != 0) ) {
                dev_err(p_msg_data->dev, "ucext_msg_update_volts[%d] response header (0x%02x,0x%02x) invalid\n", devid,
                         p_msg_data->rx_buff.rsp_msg.rsp_hdr.result, p_msg_data->rx_buff.rsp_msg.rsp_hdr.len);
                retval = -EIO;
                goto error;
            }

            // Get the Vouts 
            pregs = (__s16 *)(&p_msg_data->rx_buff.rsp_msg.rsp_data[5]);
            for(i=0; i<(NUM_UCEXT_TOMAHAWK_VOLTS); i++) {
                p_volt_vals[i] = msp430_volts_milli(*pregs++);
            }

            // Get the PWR Good bits if available (and sign extend to higher bits)
            if(p_msg_data->rx_buff.rsp_msg.rsp_data[4]==1) {
                pregs = (__u16 *)(&p_msg_data->rx_buff.rsp_msg.rsp_data[21]);
                if(pwr_test) {
                    *pwr_good = ~pwr_test;
                }
                else {
                    *pwr_good = (*pregs)|0xffff0000L;
                }
            }
            break;

        /* Update Volts for MSP430 Denverton device */
        case UCEXT_DENVERTON_DEVICE_INDEX:
            p_msg_data->tx_buff.cmd_msg.cmd_hdr.len = sizeof(t_ucext_cmd_hdr) + 1;
            p_msg_data->tx_buff.cmd_msg.cmd_hdr.chksum = UCMD_GET_MSP430 + 2;
            p_msg_data->tx_buff.cmd_msg.cmd_hdr.cmd =  UCMD_GET_MSP430;
            p_msg_data->tx_buff.cmd_msg.cmd_data[0] =  (2);
            p_msg_data->tx_cnt = p_msg_data->tx_buff.cmd_msg.cmd_hdr.len;

            memset((void *)&p_msg_data->rx_buff,0,sizeof(t_ucext_response));  
            p_msg_data->rx_cnt = 0;
            retval = ucext_cmd_response(p_msg_data->dev, p_msg_data->tx_cnt, (unsigned char *)&p_msg_data->tx_buff, 
                                        (unsigned char *)&p_msg_data->rx_buff, &p_msg_data->rx_cnt, p_msg_data->timeout_msec);

            if(retval!=0) {
                dev_info(p_msg_data->dev, "ucext_msg_update_volts[%d] cmd response failure! cmd=(0x%02x,0x%02x 0x%02x 0x%02x) invalid\n", devid,
                        p_msg_data->tx_buff.cmd_msg.cmd_hdr.len, p_msg_data->tx_buff.cmd_msg.cmd_hdr.chksum,
                        p_msg_data->tx_buff.cmd_msg.cmd_hdr.cmd, p_msg_data->tx_buff.cmd_msg.cmd_data[0]);
                goto error;
            }

            // Check for ACK, length, and num voltages
            if( (p_msg_data->rx_buff.rsp_msg.rsp_hdr.result != URSP_ACK) ||
                (p_msg_data->rx_buff.rsp_msg.rsp_hdr.len != 27) || 
                (p_msg_data->rx_buff.rsp_msg.rsp_data[0] != 10) ||
                (p_msg_data->rx_buff.rsp_msg.rsp_data[3] != 0) ) {
                dev_err(p_msg_data->dev, "ucext_msg_update_volts[%d] response header (0x%02x,0x%02x) invalid\n", devid,
                         p_msg_data->rx_buff.rsp_msg.rsp_hdr.result, p_msg_data->rx_buff.rsp_msg.rsp_hdr.len);
                retval = -EIO;
                goto error;
            }

            // Get the Vouts 
            pregs = (__s16 *)(&p_msg_data->rx_buff.rsp_msg.rsp_data[5]);
            for(i=0; i<(NUM_UCEXT_DENVERTON_VOLTS); i++) {
                p_volt_vals[i] = msp430_volts_milli(*pregs++);
            }
            // No PWR Good bits 

            break;

        default:
            retval = 0;
            break;
    }

    error:
    mutex_unlock(&p_msg_data->update_lock);
#endif
    return retval;
}


int ucext_msg_update_fans(int devid, int protocol, int cfg_fans, void *p_data, int *p_fan_vals, int *p_pres_vals) {
    int retval = 0;
    struct ucext_msg_data *p_msg_data = p_data;
    int i;
    __s16 *pregs; 
    unsigned char present_bits;

    /* Ingore Aux devices that do not support FANS*/
    if(devid!=UCEXT_TIVAC_DEVICE_INDEX) {
        return 0;
    }

    if(!p_msg_data || !p_fan_vals) {
        dev_err(p_msg_data->dev, "ucext_msg_update_fans null pointer to private data\n");
        return -EINVAL;
    }

    if(!p_msg_data->valid) {
        dev_err(p_msg_data->dev, "ucext_msg_update_fans private data not valid\n");
        return -ENODEV;
    }

    if(p_msg_data->msg_restrict!=0) {
        return 0;
    }

    mutex_lock(&p_msg_data->update_lock);

    p_msg_data->tx_buff.cmd_msg.cmd_hdr.len = sizeof(t_ucext_cmd_hdr);
    p_msg_data->tx_buff.cmd_msg.cmd_hdr.chksum = UCMD_UPDATE_FANS;
    p_msg_data->tx_buff.cmd_msg.cmd_hdr.cmd =  UCMD_UPDATE_FANS;
    p_msg_data->tx_cnt = sizeof(t_ucext_cmd_hdr);

    memset((void *)&p_msg_data->rx_buff,0,sizeof(t_ucext_response));  
    p_msg_data->rx_cnt = 0;
    retval = ucext_cmd_response(p_msg_data->dev, p_msg_data->tx_cnt, (unsigned char *)&p_msg_data->tx_buff, 
                                (unsigned char *)&p_msg_data->rx_buff, &p_msg_data->rx_cnt, p_msg_data->timeout_msec);

    if(retval!=0) {
        dev_info(p_msg_data->dev, "ucext_msg_update_fans cmd response failure\n");
        retval = -EIO;
        goto error;
    }

    if( (p_msg_data->rx_buff.rsp_msg.rsp_hdr.result != URSP_ACK))
    {
        dev_info(p_msg_data->dev, "ucext_msg_update_fans response header (0x%02x,0x%02x) invalid\n", 
                 p_msg_data->rx_buff.rsp_msg.rsp_hdr.result, p_msg_data->rx_buff.rsp_msg.rsp_hdr.len);
        retval = -EIO;
        goto error;
    }

    // if( (p_msg_data->rx_buff.rsp_msg.rsp_hdr.result != URSP_ACK) ||  
    //     (p_msg_data->rx_buff.rsp_msg.rsp_hdr.len<((sizeof(t_ucext_rsp_hdr)-1)+(2*cfg_fans))) ) {
    //     dev_info(p_msg_data->dev, "ucext_msg_update_fans response header (0x%02x,0x%02x) invalid\n", 
    //              p_msg_data->rx_buff.rsp_msg.rsp_hdr.result, p_msg_data->rx_buff.rsp_msg.rsp_hdr.len);
    //     retval = -EIO;
    //     goto error;
    // }
    

    pregs = (__u16 *)p_msg_data->rx_buff.rsp_msg.rsp_data;
    for(i=0; i<NUM_UCEXT_TIVAC_FANS; i++) {
        if(i<cfg_fans) {
            p_fan_vals[i] = *pregs++;  
        }
        else {
            p_fan_vals[i] = 0;  
        }
    }

    if( (p_msg_data->rx_buff.rsp_msg.rsp_hdr.result == URSP_ACK) &&  
        (p_msg_data->rx_buff.rsp_msg.rsp_hdr.len>((sizeof(t_ucext_rsp_hdr)-1)+(2*NUM_UCEXT_TIVAC_FANS))) &&
        (protocol> UCEXT_MSG_MIN_PROTOCOL) ) {
        present_bits=p_msg_data->rx_buff.rsp_msg.rsp_data[2*NUM_UCEXT_TIVAC_FANS];
        for(i=0; i<NUM_UCEXT_TIVAC_FANS; i++) {
            if( (((present_bits>>i)&1)!=0) && (i<cfg_fans) ) {
                p_pres_vals[i] = 1;  
            }
            else {
                p_pres_vals[i] = 0;  
            }
        }
    }

    error:
    mutex_unlock(&p_msg_data->update_lock);
    return retval;
}


int ucext_msg_update_buttons(int devid, void *p_data, int *p_button_vals) {
    int retval = 0;
    struct ucext_msg_data *p_msg_data = p_data;
    int i;

    /* Ingore Aux devices that do not support BUTTONS*/
    if(devid!=UCEXT_TIVAC_DEVICE_INDEX) {
        return 0;
    }

    if(!p_msg_data || !p_button_vals) {
        dev_err(p_msg_data->dev, "ucext_msg_update_buttons null pointer to private data\n");
        return -EINVAL;
    }

    if(!p_msg_data->valid) {
        dev_err(p_msg_data->dev, "ucext_msg_update_buttons private data not valid\n");
        return -ENODEV;
    }

// LTG Temporary block of update buttons
    if(p_msg_data->msg_restrict!=0 || p_msg_data->msg_restrict==0) {
        return 0;
    }

    mutex_lock(&p_msg_data->update_lock);

#if defined(NUM_UCEXT_TIVAC_BUTTONS) && (NUM_UCEXT_TIVAC_BUTTONS>1)
#error "NUM_UCEXT_TIVAC_BUTTONS requested exceeds that currently supported by Tivac MSG protocol "
#endif

    p_msg_data->tx_buff.cmd_msg.cmd_hdr.len = sizeof(t_ucext_cmd_hdr);
    p_msg_data->tx_buff.cmd_msg.cmd_hdr.chksum = UCMD_GET_BUTTON;
    p_msg_data->tx_buff.cmd_msg.cmd_hdr.cmd =  UCMD_GET_BUTTON;
    p_msg_data->tx_cnt = sizeof(t_ucext_cmd_hdr);

    memset((void *)&p_msg_data->rx_buff,0,sizeof(t_ucext_response));  
    p_msg_data->rx_cnt = 0;
    retval = ucext_cmd_response(p_msg_data->dev, p_msg_data->tx_cnt, (unsigned char *)&p_msg_data->tx_buff, 
                                (unsigned char *)&p_msg_data->rx_buff, &p_msg_data->rx_cnt, p_msg_data->timeout_msec);

    if(retval!=0) {
        dev_info(p_msg_data->dev, "ucext_msg_update_buttons cmd response failure\n");
        retval = -EIO;
        goto error;
    }

    if( (p_msg_data->rx_buff.rsp_msg.rsp_hdr.result != URSP_ACK) ||  
        (p_msg_data->rx_buff.rsp_msg.rsp_hdr.len!=((sizeof(t_ucext_rsp_hdr)-1)+(NUM_UCEXT_TIVAC_BUTTONS))) ) {
        dev_info(p_msg_data->dev, "ucext_msg_update_buttons response header (0x%02x,0x%02x) invalid\n", 
                 p_msg_data->rx_buff.rsp_msg.rsp_hdr.result, p_msg_data->rx_buff.rsp_msg.rsp_hdr.len);
        retval = -EIO;
        goto error;
    }

    for(i=0; i<NUM_UCEXT_TIVAC_BUTTONS; i++) {
        p_button_vals[i] = (int)p_msg_data->rx_buff.rsp_msg.rsp_data[i];  
    }

    error:
    mutex_unlock(&p_msg_data->update_lock);
    return retval;
}


int ucext_msg_update_aux_status(int devid, void *p_data, int *p_aux_vals) {
    int retval = 0;
    struct ucext_msg_data *p_msg_data = p_data;

    if(!p_msg_data || !p_aux_vals) {
        dev_err(p_msg_data->dev, "ucext_msg_update_aux_status null pointer to private data\n");
        return -EINVAL;
    }

    if(!p_msg_data->valid) {
        dev_err(p_msg_data->dev, "ucext_msg_update_aux_status private data not valid\n");
        return -ENODEV;
    }

    if(p_msg_data->msg_restrict!=0) {
        return 0;
    }

#ifndef UCEXT_MSG_STUB
    int *p_vals = p_aux_vals;
    int i = 0;
    int bitfaults = 0;

    mutex_lock(&p_msg_data->update_lock);

    p_msg_data->tx_buff.cmd_msg.cmd_hdr.len = sizeof(t_ucext_cmd_hdr);
    p_msg_data->tx_buff.cmd_msg.cmd_hdr.chksum = UCMD_GET_STATUS;
    p_msg_data->tx_buff.cmd_msg.cmd_hdr.cmd =  UCMD_GET_STATUS;
    p_msg_data->tx_cnt = sizeof(t_ucext_cmd_hdr);

    memset((void *)&p_msg_data->rx_buff,0,sizeof(t_ucext_response));  
    p_msg_data->rx_cnt = 0;
    retval = ucext_cmd_response(p_msg_data->dev, p_msg_data->tx_cnt, (unsigned char *)&p_msg_data->tx_buff, 
                                (unsigned char *)&p_msg_data->rx_buff, &p_msg_data->rx_cnt, p_msg_data->timeout_msec );

    if(retval!=0) {
        dev_info(p_msg_data->dev, "ucext_msg_update_aux_status cmd response failure\n");
        retval = -EIO;
        goto error;
    }

    if( (p_msg_data->rx_buff.rsp_msg.rsp_hdr.result!=URSP_ACK) ||
        ((p_msg_data->rx_buff.rsp_msg.rsp_hdr.len<((sizeof(t_ucext_rsp_hdr)-1)+24))) ) {
        dev_info(p_msg_data->dev, "ucext_msg_update_aux_status response header invalid\n");
        retval = -EIO;
        goto error;
    }
//  else
//  {
//      dev_info(p_msg_data->dev, "ucext_msg_update_aux_status cmd success stat=%d len=%d\n",
//               p_msg_data->rx_buff.rsp_msg.rsp_hdr.result,
//               p_msg_data->rx_buff.rsp_msg.rsp_hdr.len );
//  }

    switch(devid) {
        case UCEXT_TIVAC_DEVICE_INDEX:
            for(i=0;i<16;i++) {
                if(p_msg_data->rx_buff.rsp_msg.rsp_data[i]!=0) {
                    bitfaults |= 1<<i;
                }
            }
            *p_vals++ = bitfaults;                                       // chip_dev_status (none for tiva yet so bit map aux faults)
            *p_vals++ = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[0];   // chip_aux_dev1  
            *p_vals++ = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[1];   // chip_aux_dev2  
            *p_vals++ = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[2];   // chip_aux_dev3  
            *p_vals++ = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[3];   // chip_aux_dev4  
            *p_vals++ = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[4];   // chip_aux_clk1  
            *p_vals++ = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[5];   // chip_aux_clk2  
            *p_vals++ = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[6];   // chip_aux_prog1 
            *p_vals++ = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[7];   // chip_aux_therm0
            *p_vals++ = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[8];   // chip_aux_therm1
            *p_vals++ = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[9];   // chip_aux_therm2
            *p_vals++ = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[10];  // chip_aux_therm3
            *p_vals++ = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[11];  // chip_aux_therm4
            *p_vals++ = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[12];  // chip_aux_therm5
            *p_vals++ = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[13];  // chip_aux_therm6
            *p_vals++ = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[14];  // chip_aux_therm7
            *p_vals++ = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[15];  // chip_aux_therm8

            *p_vals = (int)p_msg_data->rx_buff.rsp_msg.rsp_data[16];     // chip_dev_rxpkts
            *p_vals |= ((int)p_msg_data->rx_buff.rsp_msg.rsp_data[17])<<8;
            *p_vals |= ((int)p_msg_data->rx_buff.rsp_msg.rsp_data[18])<<16;
            *p_vals |= ((int)p_msg_data->rx_buff.rsp_msg.rsp_data[19])<<24;
            p_vals++;
            *p_vals = (int)p_msg_data->rx_buff.rsp_msg.rsp_data[20];     // chip_dev_txpkts
            *p_vals |= ((int)p_msg_data->rx_buff.rsp_msg.rsp_data[21])<<8;
            *p_vals |= ((int)p_msg_data->rx_buff.rsp_msg.rsp_data[22])<<16;
            *p_vals |= ((int)p_msg_data->rx_buff.rsp_msg.rsp_data[23])<<24;
            p_vals++;
            break;
        case UCEXT_TOMAHAWK_DEVICE_INDEX:
            *p_vals++ = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[1];   // chip_dev_status=chip_aux_dev2
            break;                                                              
        case UCEXT_DENVERTON_DEVICE_INDEX:
            *p_vals++ = (int) p_msg_data->rx_buff.rsp_msg.rsp_data[2];   // chip_dev_status=chip_aux_dev3
            break;                                                              
        default:
            retval = 0;
            break;
    }
    error:
    mutex_unlock(&p_msg_data->update_lock);
#endif
    return retval;
}


int ucext_msg_update_leds(int devid, int protocol, int cfg_leds, void *p_data, int *p_color_vals, int *p_rate_vals ) {
    int retval = 0;
    struct ucext_msg_data *p_msg_data = p_data;

    /* Ingore Aux devices that do not support LEDS*/
    if(devid!=UCEXT_TIVAC_DEVICE_INDEX) {
        return 0;
    }

    if(!p_msg_data || !p_rate_vals) {
        dev_err(p_msg_data->dev, "ucext_msg_update_leds null pointer to private data\n");
        return -EINVAL;
    }

    if(!p_msg_data->valid) {
        dev_err(p_msg_data->dev, "ucext_msg_update_leds private data not valid\n");
        return -ENODEV;
    }

    if(p_msg_data->msg_restrict!=0) {
        return 0;
    }

#ifndef UCEXT_MSG_STUB
    int i;

    mutex_lock(&p_msg_data->update_lock);

    p_msg_data->tx_buff.cmd_msg.cmd_hdr.len = sizeof(t_ucext_cmd_hdr);
    p_msg_data->tx_buff.cmd_msg.cmd_hdr.cmd =  UCMD_GET_LEDS;
    p_msg_data->tx_buff.cmd_msg.cmd_hdr.chksum = UCMD_GET_LEDS;
    p_msg_data->tx_cnt = sizeof(t_ucext_cmd_hdr);

    memset((void *)&p_msg_data->rx_buff,0,sizeof(t_ucext_response));  
    p_msg_data->rx_cnt = 0;
    retval = ucext_cmd_response(p_msg_data->dev, p_msg_data->tx_cnt, (unsigned char *)&p_msg_data->tx_buff, 
                                (unsigned char *)&p_msg_data->rx_buff, &p_msg_data->rx_cnt, p_msg_data->timeout_msec);

    if(retval!=0) {
        dev_info(p_msg_data->dev, "ucext_msg_update_leds cmd response failure\n");
        retval = -EIO;
        goto error;
    }

    if(p_msg_data->rx_buff.rsp_msg.rsp_hdr.result != URSP_ACK) {
        dev_info(p_msg_data->dev, "ucext_msg_update_leds response header (0x%02x,0x%02x) invalid\n", 
                 p_msg_data->rx_buff.rsp_msg.rsp_hdr.result, p_msg_data->rx_buff.rsp_msg.rsp_hdr.len);
        retval = -EIO;
        goto error;
    }

    for(i=0;i<NUM_UCEXT_TIVAC_LEDS;i++) {
        if(i<cfg_leds) {
            p_color_vals[i] = p_msg_data->rx_buff.rsp_msg.rsp_data[(3*i)+0];     // get LED color setting
            p_rate_vals[i] =  p_msg_data->rx_buff.rsp_msg.rsp_data[(3*i)+1];     // get LED blink rate setting
        }
        else {
            p_color_vals[i] = 0;
            p_rate_vals[i] =  0;
        }

    }

    error:
    mutex_unlock(&p_msg_data->update_lock);
#endif
    return retval;
}


int ucext_msg_set_pwms(int devid, void *p_data, int *p_pwmen_vals, int *p_pwm_vals ) {
    int retval = 0;
    struct ucext_msg_data *p_msg_data = p_data;
    int i;

    /* Ingore Aux devices that do not support PWMS*/
    if(devid!=UCEXT_TIVAC_DEVICE_INDEX) {
        return 0;
    }

    if(!p_msg_data || !p_pwm_vals) {
        dev_err(p_msg_data->dev, "ucext_msg_set_pwms null pointer to private data\n");
        return -EINVAL;
    }

    if(!p_msg_data->valid) {
        dev_err(p_msg_data->dev, "ucext_msg_set_pwms private data not valid\n");
        return -ENODEV;
    }

// LTG Temporary block of pwms
    // if(p_msg_data->msg_restrict!=0 || p_msg_data->msg_restrict==0) {
    //     return 0;
    // }

   if(p_msg_data->msg_restrict!=0) {
        return 0;
    }

    mutex_lock(&p_msg_data->update_lock);

    for(i=0;i<NUM_UCEXT_TIVAC_PWMS;i++) {
        p_msg_data->tx_buff.cmd_msg.cmd_hdr.cmd =  UCMD_SET_PWM;
        p_msg_data->tx_buff.cmd_msg.cmd_data[0] = i+1;    // Base 1 index
        p_msg_data->tx_buff.cmd_msg.cmd_data[1] = (__u8) p_pwmen_vals[i];   // enable
        p_msg_data->tx_buff.cmd_msg.cmd_data[2] = (__u8) p_pwm_vals[i];     // pwm speed
        p_msg_data->tx_buff.cmd_msg.cmd_hdr.chksum = UCMD_SET_PWM;
        for(i=0;i<3;i++) {
            p_msg_data->tx_buff.cmd_msg.cmd_hdr.chksum+=p_msg_data->tx_buff.cmd_msg.cmd_data[i];
        }
        p_msg_data->tx_buff.cmd_msg.cmd_hdr.len = sizeof(t_ucext_cmd_hdr)+3;
        p_msg_data->tx_cnt = sizeof(t_ucext_cmd_hdr)+3;

        memset((void *)&p_msg_data->rx_buff,0,sizeof(t_ucext_response));  
        p_msg_data->rx_cnt = 0;
        retval = ucext_cmd_response(p_msg_data->dev, p_msg_data->tx_cnt, (unsigned char *)&p_msg_data->tx_buff, 
                                    (unsigned char *)&p_msg_data->rx_buff, &p_msg_data->rx_cnt, p_msg_data->timeout_msec);

        if(retval!=0) {
            dev_info(p_msg_data->dev, "ucext_msg_set_pwms cmd response failure\n");
            retval = -EIO;
            goto error;
        }

        if( (p_msg_data->rx_buff.rsp_msg.rsp_hdr.result != URSP_ACK) ||  
            (p_msg_data->rx_buff.rsp_msg.rsp_hdr.len!=0) ) {
            dev_info(p_msg_data->dev, "ucext_msg_set_pwms response header (0x%02x,0x%02x) invalid\n", 
                     p_msg_data->rx_buff.rsp_msg.rsp_hdr.result, p_msg_data->rx_buff.rsp_msg.rsp_hdr.len);
            retval = -EIO;
            goto error;
        }
    }

    error:
    mutex_unlock(&p_msg_data->update_lock);
    return retval;
}

int ucext_msg_set_leds(int devid, void *p_data, int *p_color_vals, int *p_rate_vals ) {
    int retval = 0;
    struct ucext_msg_data *p_msg_data = p_data;

    /* Ingore Aux devices that do not support LEDS*/
    if(devid!=UCEXT_TIVAC_DEVICE_INDEX) {
        return 0;
    }

    if(!p_msg_data || !p_rate_vals) {
        dev_err(p_msg_data->dev, "ucext_msg_set_leds null pointer to private data\n");
        return -EINVAL;
    }

    if(!p_msg_data->valid) {
        dev_err(p_msg_data->dev, "ucext_msg_set_leds private data not valid\n");
        return -ENODEV;
    }

    if(p_msg_data->msg_restrict!=0) {
        return 0;
    }

#ifndef UCEXT_MSG_STUB
    int i;

    mutex_lock(&p_msg_data->update_lock);

    p_msg_data->tx_buff.cmd_msg.cmd_hdr.cmd =  UCMD_SET_LEDS;
    p_msg_data->tx_buff.cmd_msg.cmd_hdr.chksum = UCMD_SET_LEDS;
    for(i=0;i<NUM_UCEXT_TIVAC_LEDS;i++) {
        p_msg_data->tx_buff.cmd_msg.cmd_data[(2*i)+0] = (__u8) p_color_vals[i];    // LED color setting
        p_msg_data->tx_buff.cmd_msg.cmd_hdr.chksum += (__u8) p_color_vals[i];;

        p_msg_data->tx_buff.cmd_msg.cmd_data[(2*i)+1] = (__u8) p_rate_vals[i];     // LED blink rate setting
        p_msg_data->tx_buff.cmd_msg.cmd_hdr.chksum += (__u8) p_rate_vals[i];
    }
    p_msg_data->tx_buff.cmd_msg.cmd_hdr.len = sizeof(t_ucext_cmd_hdr)+(2*NUM_UCEXT_TIVAC_LEDS);
    p_msg_data->tx_cnt = sizeof(t_ucext_cmd_hdr)+(2*NUM_UCEXT_TIVAC_LEDS);

    memset((void *)&p_msg_data->rx_buff,0,sizeof(t_ucext_response));  
    p_msg_data->rx_cnt = 0;
    retval = ucext_cmd_response(p_msg_data->dev, p_msg_data->tx_cnt, (unsigned char *)&p_msg_data->tx_buff, 
                                (unsigned char *)&p_msg_data->rx_buff, &p_msg_data->rx_cnt, p_msg_data->timeout_msec);

    if(retval!=0) {
        dev_info(p_msg_data->dev, "ucext_msg_set_leds cmd response failure\n");
        retval = -EIO;
        goto error;
    }

    if( (p_msg_data->rx_buff.rsp_msg.rsp_hdr.result != URSP_ACK) ||  
        (p_msg_data->rx_buff.rsp_msg.rsp_hdr.len!=0) ) {
        dev_info(p_msg_data->dev, "ucext_msg_set_leds response header (0x%02x,0x%02x) invalid\n", 
                 p_msg_data->rx_buff.rsp_msg.rsp_hdr.result, p_msg_data->rx_buff.rsp_msg.rsp_hdr.len);
        retval = -EIO;
        goto error;
    }

    error:
    mutex_unlock(&p_msg_data->update_lock);
#endif
    return retval;
}


int ucext_msg_clear_button(int devid, void *p_data, int button ) {
    int retval = 0;
    struct ucext_msg_data *p_msg_data = p_data;

    /* Ingore Aux devices that do not support BUTTONS*/
    if(devid!=UCEXT_TIVAC_DEVICE_INDEX) {
        return 0;
    }

    if(!p_msg_data) {
        dev_err(p_msg_data->dev, "ucext_msg_clear_button null pointer to private message data\n");
        return -EINVAL;
    }

    if(!p_msg_data->valid) {
        dev_err(p_msg_data->dev, "ucext_msg_clear_button private data not valid\n");
        return -ENODEV;
    }

// LTG Temporary block of clear button
    if(p_msg_data->msg_restrict!=0 || p_msg_data->msg_restrict==0) {
        return 0;
    }

#if defined(NUM_UCEXT_TIVAC_BUTTONS) && (NUM_UCEXT_TIVAC_BUTTONS>1)
#error "NUM_UCEXT_TIVAC_BUTTONS requested exceeds that currently supported by Tivac MSG protocol "
#endif

    /* Note currently the button is a don't care since only 1 */
    mutex_lock(&p_msg_data->update_lock);

    p_msg_data->tx_buff.cmd_msg.cmd_hdr.len = sizeof(t_ucext_cmd_hdr);
    p_msg_data->tx_buff.cmd_msg.cmd_hdr.chksum = UCMD_CLR_BUTTON;
    p_msg_data->tx_buff.cmd_msg.cmd_hdr.cmd =  UCMD_CLR_BUTTON;
    p_msg_data->tx_cnt = sizeof(t_ucext_cmd_hdr);

    memset((void *)&p_msg_data->rx_buff,0,sizeof(t_ucext_response));  
    p_msg_data->rx_cnt = 0;
    retval = ucext_cmd_response(p_msg_data->dev, p_msg_data->tx_cnt, (unsigned char *)&p_msg_data->tx_buff, 
                                (unsigned char *)&p_msg_data->rx_buff, &p_msg_data->rx_cnt, p_msg_data->timeout_msec);

    if(retval!=0) {
        dev_info(p_msg_data->dev, "ucext_msg_clear_button cmd response failure\n");
        retval = -EIO;
        goto error;
    }

    if( (p_msg_data->rx_buff.rsp_msg.rsp_hdr.result!=URSP_ACK) ||
        (p_msg_data->rx_buff.rsp_msg.rsp_hdr.len!=0) ) {
        dev_info(p_msg_data->dev, "ucext_msg_clear_button response header invalid\n");
        retval = -EIO;
        goto error;
    }

    error:
    mutex_unlock(&p_msg_data->update_lock);
    return retval;
}


int ucext_msg_set_reboot(int devid, void *p_data, int *p_reboot_val) {
    int retval = 0;
    struct ucext_msg_data *p_msg_data = p_data;

    /* Ingore Aux devices that do not support BUTTONS*/
    if(devid!=UCEXT_TIVAC_DEVICE_INDEX) {
        return 0;
    }

    if(!p_msg_data || !p_reboot_val) {
        dev_err(p_msg_data->dev, "ucext_msg_set_reboot null pointer to private data\n");
        return -EINVAL;
    }

    if(!p_msg_data->valid) {
        dev_err(p_msg_data->dev, "ucext_msg_set_reboot private data not valid\n");
        return -ENODEV;
    }

    if( (*p_reboot_val<0)||(*p_reboot_val>1) ) {
        dev_err(p_msg_data->dev, "ucext_msg_set_reboot invalid value\n");
        return -EINVAL;
    }

    if(p_msg_data->msg_restrict!=0) {
        dev_err(p_msg_data->dev, "ucext_msg_set_reboot blocked due to restricted messages\n");
        return 0;
    }

#ifndef UCEXT_MSG_STUB
    /* Note currently the button is a don't care since only 1 */
    mutex_lock(&p_msg_data->update_lock);

    dev_info(p_msg_data->dev, "ALERT tivac ucext_msg_set_reboot sending cmd %d by pid %d!\n", *p_reboot_val, (int)task_pid_nr(current));

    p_msg_data->tx_buff.cmd_msg.cmd_hdr.len = sizeof(t_ucext_cmd_hdr)+1;
    p_msg_data->tx_buff.cmd_msg.cmd_hdr.chksum = UCMD_SET_RESET;
    p_msg_data->tx_buff.cmd_msg.cmd_hdr.cmd =  UCMD_SET_RESET;
    p_msg_data->tx_buff.cmd_msg.cmd_data[0] = (__u8) *p_reboot_val;
    p_msg_data->tx_buff.cmd_msg.cmd_hdr.chksum+=p_msg_data->tx_buff.cmd_msg.cmd_data[0];
    p_msg_data->tx_cnt = sizeof(t_ucext_cmd_hdr)+1;

    memset((void *)&p_msg_data->rx_buff,0,sizeof(t_ucext_response));  
    p_msg_data->rx_cnt = 0;
    retval = ucext_cmd_response(p_msg_data->dev, p_msg_data->tx_cnt, (unsigned char *)&p_msg_data->tx_buff, 
                                (unsigned char *)&p_msg_data->rx_buff, &p_msg_data->rx_cnt, p_msg_data->timeout_msec);

    if(retval!=0) {
        dev_info(p_msg_data->dev, "ucext_msg_set_reboot cmd response failure\n");
        retval = -EIO;
        goto error;
    }

    if( (p_msg_data->rx_buff.rsp_msg.rsp_hdr.result!=URSP_ACK) ||
        (p_msg_data->rx_buff.rsp_msg.rsp_hdr.len!=0) ) {
        dev_info(p_msg_data->dev, "ucext_msg_set_reboot response header invalid\n");
        retval = -EIO;
        goto error;
    }

    error:
    mutex_unlock(&p_msg_data->update_lock);
#endif
    return retval;
}


int ucext_msg_set_watchdog(int devid, void *p_data, int *p_watchdog_val) {
    int retval = 0;
    struct ucext_msg_data *p_msg_data = p_data;

    /* Ingore Aux devices that do not support BUTTONS*/
    if(devid!=UCEXT_TIVAC_DEVICE_INDEX) {
        return 0;
    }

    if(!p_msg_data || !p_watchdog_val) {
        dev_err(p_msg_data->dev, "ucext_msg_set_watchdog null pointer to private data\n");
        return -EINVAL;
    }

    if(!p_msg_data->valid) {
        dev_err(p_msg_data->dev, "ucext_msg_set_watchdog private data not valid\n");
        return -ENODEV;
    }

    if( (*p_watchdog_val<0)||(*p_watchdog_val>255) ) {
        dev_err(p_msg_data->dev, "ucext_msg_set_watchdog invalid value\n");
        return -EINVAL;
    }

    if(p_msg_data->msg_restrict!=0) {
        dev_err(p_msg_data->dev, "ucext_msg_set_watchdog blocked due to restricted messages\n");
        return 0;
    }

#ifndef UCEXT_MSG_STUB
    /* Note currently the button is a don't care since only 1 */
    mutex_lock(&p_msg_data->update_lock);

    dev_info(p_msg_data->dev, "ALERT tivac ucext_msg_set_watchdog to %d minutes by pid %d!\n", *p_watchdog_val, (int)task_pid_nr(current));

    p_msg_data->tx_buff.cmd_msg.cmd_hdr.len = sizeof(t_ucext_cmd_hdr)+1;
    p_msg_data->tx_buff.cmd_msg.cmd_hdr.chksum = UCMD_SET_WATCHDOG;
    p_msg_data->tx_buff.cmd_msg.cmd_hdr.cmd =  UCMD_SET_WATCHDOG;
    p_msg_data->tx_buff.cmd_msg.cmd_data[0] = (__u8) *p_watchdog_val;
    p_msg_data->tx_buff.cmd_msg.cmd_hdr.chksum+=p_msg_data->tx_buff.cmd_msg.cmd_data[0];
    p_msg_data->tx_cnt = sizeof(t_ucext_cmd_hdr)+1;

    memset((void *)&p_msg_data->rx_buff,0,sizeof(t_ucext_response));  
    p_msg_data->rx_cnt = 0;
    retval = ucext_cmd_response(p_msg_data->dev, p_msg_data->tx_cnt, (unsigned char *)&p_msg_data->tx_buff, 
                                (unsigned char *)&p_msg_data->rx_buff, &p_msg_data->rx_cnt, p_msg_data->timeout_msec);

    if(retval!=0) {
        dev_info(p_msg_data->dev, "ucext_msg_set_watchdog cmd response failure\n");
        retval = -EIO;
        goto error;
    }

    if( (p_msg_data->rx_buff.rsp_msg.rsp_hdr.result!=URSP_ACK) ||
        (p_msg_data->rx_buff.rsp_msg.rsp_hdr.len!=0) ) {
        dev_info(p_msg_data->dev, "ucext_msg_set_watchdog response header invalid\n");
        retval = -EIO;
        goto error;
    }

    error:
    mutex_unlock(&p_msg_data->update_lock);
#endif
    return retval;
}

int ucext_msg_clear_pwrgood(int devid, void *p_data) {
    int retval = 0;
    struct ucext_msg_data *p_msg_data = p_data;

    /* Ingore Aux devices that do not support BUTTONS*/
    if(devid!=UCEXT_TIVAC_DEVICE_INDEX) {
        return 0;
    }

    if(!p_msg_data) {
        dev_err(p_msg_data->dev, "ucext_msg_clear_pwrgood null pointer to private data\n");
        return -EINVAL;
    }

    if(!p_msg_data->valid) {
        dev_err(p_msg_data->dev, "ucext_msg_clear_pwrgood private data not valid\n");
        return -ENODEV;
    }

    if(p_msg_data->msg_restrict!=0) {
        return 0;
    }

#ifndef UCEXT_MSG_STUB
    /* Note currently the button is a don't care since only 1 */
    mutex_lock(&p_msg_data->update_lock);

    p_msg_data->tx_buff.cmd_msg.cmd_hdr.len = sizeof(t_ucext_cmd_hdr)+1;
    p_msg_data->tx_buff.cmd_msg.cmd_hdr.chksum = UCMD_CLR_PWRGOOD;
    p_msg_data->tx_buff.cmd_msg.cmd_hdr.cmd =  UCMD_CLR_PWRGOOD;
    p_msg_data->tx_buff.cmd_msg.cmd_data[0] = (__u8)devid;
    p_msg_data->tx_buff.cmd_msg.cmd_hdr.chksum+=p_msg_data->tx_buff.cmd_msg.cmd_data[0];
    p_msg_data->tx_cnt = sizeof(t_ucext_cmd_hdr)+1;

    memset((void *)&p_msg_data->rx_buff,0,sizeof(t_ucext_response));  
    p_msg_data->rx_cnt = 0;
    retval = ucext_cmd_response(p_msg_data->dev, p_msg_data->tx_cnt, (unsigned char *)&p_msg_data->tx_buff, 
                                (unsigned char *)&p_msg_data->rx_buff, &p_msg_data->rx_cnt, p_msg_data->timeout_msec);

    if(retval!=0) {
        dev_info(p_msg_data->dev, "ucext_msg_clear_pwrgood cmd response failure\n");
        retval = -EIO;
        goto error;
    }

    if( (p_msg_data->rx_buff.rsp_msg.rsp_hdr.result!=URSP_ACK) ||
        (p_msg_data->rx_buff.rsp_msg.rsp_hdr.len!=0) ) {
        dev_info(p_msg_data->dev, "ucext_msg_clear_pwrgood response header invalid\n");
        retval = -EIO;
        goto error;
    }

    error:
    mutex_unlock(&p_msg_data->update_lock);
#endif
    return retval;
}


int ucext_msg_set_msg_protocol(int devid, void *p_data, int protocol) {
    int retval = 0;
    struct ucext_msg_data *p_msg_data = p_data;

    /* Ingore Aux devices that do not support BUTTONS*/
    if(devid!=UCEXT_TIVAC_DEVICE_INDEX) {
        return 0;
    }

    if(!p_msg_data) {
        dev_err(p_msg_data->dev, "ucext_msg_set_msg_protocol null pointer to private data\n");
        return -EINVAL;
    }

    if(!p_msg_data->valid) {
        dev_err(p_msg_data->dev, "ucext_msg_set_msg_protocol private data not valid\n");
        return -ENODEV;
    }

    if( (protocol< UCEXT_MSG_MIN_PROTOCOL)||(protocol> UCEXT_MSG_MAX_PROTOCOL) ) {
        dev_err(p_msg_data->dev, "ucext_msg_set_msg_protocol invalid value\n");
        return -EINVAL;
    }

    if(p_msg_data->msg_restrict!=0) {
        dev_err(p_msg_data->dev, "ucext_msg_set_msg_protocol blocked due to restricted messages\n");
        return 0;
    }

#ifndef UCEXT_MSG_STUB
    /* Note currently the button is a don't care since only 1 */
    mutex_lock(&p_msg_data->update_lock);

    p_msg_data->tx_buff.cmd_msg.cmd_hdr.len = sizeof(t_ucext_cmd_hdr)+1;
    p_msg_data->tx_buff.cmd_msg.cmd_hdr.chksum = UCMD_SET_PROTOCOL;
    p_msg_data->tx_buff.cmd_msg.cmd_hdr.cmd =  UCMD_SET_PROTOCOL;
    p_msg_data->tx_buff.cmd_msg.cmd_data[0] = (__u8) protocol;
    p_msg_data->tx_buff.cmd_msg.cmd_hdr.chksum+=p_msg_data->tx_buff.cmd_msg.cmd_data[0];
    p_msg_data->tx_cnt = sizeof(t_ucext_cmd_hdr)+1;

    memset((void *)&p_msg_data->rx_buff,0,sizeof(t_ucext_response));  
    p_msg_data->rx_cnt = 0;
    retval = ucext_cmd_response(p_msg_data->dev, p_msg_data->tx_cnt, (unsigned char *)&p_msg_data->tx_buff, 
                                (unsigned char *)&p_msg_data->rx_buff, &p_msg_data->rx_cnt, p_msg_data->timeout_msec);

    if(retval!=0) {
        dev_info(p_msg_data->dev, "ucext_msg_set_msg_protocol cmd response failure\n");
        retval = -EIO;
        goto error;
    }

    if( (p_msg_data->rx_buff.rsp_msg.rsp_hdr.result!=URSP_ACK) ||
        (p_msg_data->rx_buff.rsp_msg.rsp_hdr.len!=0) ) {
        dev_info(p_msg_data->dev, "ucext_msg_set_msg_protocol response header invalid\n");
        retval = -EIO;
        goto error;
    }

    error:
    mutex_unlock(&p_msg_data->update_lock);
#endif
    return retval;
}


