#ifndef _UCEXT_USB_H_
#define _UCEXT_USB_H_

#ifndef UCEXT_DEBUG_MESSAGE_LEVEL
    #define UCEXT_DEBUG_MESSAGE_LEVEL   0
#endif

/* Selects the dev node file ops to use the special usb_bulk_msg with timeout ops, or the standard blocking USB bulk ops */
#define UCEXT_FOP_READ_HAS_TIMEOUT   1
#define UCEXT_FOP_WRITE_HAS_TIMEOUT  1

extern struct tivac_data *tivac_update_device(struct device *dev);

extern void *ucext_get_dev_msg_data(struct device *pdev);
extern void *ucext_get_dev_tivac_data(struct device *pdev);
extern void *ucext_get_dev_sharedpwr_data(struct device *pdev);
extern void *ucext_get_dev_tomahawk_data(struct device *pdev);
extern void *ucext_get_dev_denverton_data(struct device *pdev);

extern int ucext_get_dev_msg_debug(struct device *pdev);
extern int ucext_set_dev_msg_debug(struct device *pdev, int level);

extern int ucext_cmd_response(struct device *dev, int cmd_len, unsigned char *cmd_buff, 
                              unsigned char *rsp_buff, int *rsp_len, int timeout_msec );


#endif
