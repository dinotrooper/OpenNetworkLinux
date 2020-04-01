// SPDX-License-Identifier: GPL-2.0
/*
 * ucext-msg driver:  
 * 
 * This kernel module was heavily based on the usb-skeleton driver supplied with the kernel 
 * Copyright (C) 2001-2004 Greg Kroah-Hartman (greg@kroah.com).
 *
 * This driver is based on the 2.6.3 version of drivers/usb/usb-skeleton.c
 * but has been rewritten to be easier to read and use.
 * 
 * It was further modified to backfill specific kernel 4.9.47 changes we needed for the target.
 * Copyright (c) 2019, ADTRAN, Inc.  All rights reserved.
 */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#include <linux/usb.h>
#include <linux/mutex.h>

#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/of_device.h>
#include <linux/sysfs.h>

#include "ucext_dev_MSP430sharedpwr.h"              
#include "ucext_dev_MSP430tomahawk.h"
#include "ucext_dev_MSP430denverton.h"              
#include "ucext_dev_tivac.h"  
#include "ucext_msg.h"
#include "ucext_usb.h"                  //all these are fine

/* Define these values to match your devices */
#define UCEXT_USB_VENDOR_ID	    0x1cbe
#define UCEXT_USB_PRODUCT_ID	0x0003

/* Get a minor range for your devices from the usb maintainer */
#define UCEXT_USB_MINOR_BASE	0

/* our private defines. if this grows any larger, use your own .h file */
#define MAX_TRANSFER		(PAGE_SIZE - 512)

/* MAX_TRANSFER is chosen so that the VM is not stressed by
   allocations > PAGE_SIZE and the number of packets in a page
   is an integer 512 is the largest possible packet on EHCI */
#define WRITES_IN_FLIGHT	8
/* arbitrarily chosen */

#define MIN(a,b) (((a) <= (b)) ? (a) : (b))

/* Structure to hold all of our device specific stuff */
struct usb_ucext {
    struct usb_device    *udev;             /* the usb device for this device */
    struct usb_interface *interface;        /* the interface for this device */
    struct semaphore     limit_sem;         /* limiting the number of writes in progress */
    struct usb_anchor    submitted;         /* in case we need to retract our submissions */

    struct urb           *bulk_in_urb;      /* the urb to read data with */
    unsigned char        *bulk_in_buffer;   /* the buffer to receive data */
    size_t               bulk_in_size;      /* the size of the receive buffer */
    size_t               bulk_in_filled;    /* number of bytes in the buffer */
    size_t               bulk_in_copied;    /* already copied to user space */
    __u8                 bulk_in_endpointAddr;  /* the address of the bulk in endpoint */
    __u8                 bulk_out_endpointAddr; /* the address of the bulk out endpoint */

    int                  errors;            /* the last request tanked */
    bool                 ongoing_read;      /* a read is going on */
    spinlock_t           err_lock;          /* lock for errors */
    struct kref          kref;
    struct mutex         io_mutex;          /* synchronize I/O with disconnect */
    wait_queue_head_t    bulk_in_wait;      /* to wait for an ongoing read */
    int                  msg_debug;         /* Device message debug enable */

    void                 *p_message_data;      /* Message subsystem context*/
    void                 *p_dev_tivac_data;    /* Tivac Multi-Device subsystem context*/
    void                 *p_dev_sharedpwr_data; /* SharedPwr MSP430 Multi-Device subsystem context*/
    void                 *p_dev_tomahawk_data;   /* Tomahawk MSP430 Multi-Device subsystem context*/
    void                 *p_dev_denverton_data;     /* Denverton MSP430 Multi-Device subsystem context*/
};

#define to_ucext_dev(d) container_of(d, struct usb_ucext, kref)

/* Structure to hold all of the file specific data buffers to support async reads from different processes */
struct usb_ucext_file_data {
    struct usb_ucext    *ucextdev;
    int                  bulk_rd_cnt;        
    int                  bulk_rd_ready_cnt;        
    int                  bulk_wr_cnt;
    __u8                 bulk_rd_buff[MAX_TRANSFER];
    __u8                 bulk_wr_buff[MAX_TRANSFER];
};

//----------------------------- USB Driver --------------------------------------------------
/* table of devices that work with this driver */
static const struct usb_device_id ucext_table[] = {
    { USB_DEVICE(UCEXT_USB_VENDOR_ID, UCEXT_USB_PRODUCT_ID)},
    {}                 /* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, ucext_table);


static struct usb_driver ucext_driver;
static void ucext_draw_down(struct usb_ucext *dev);

void *ucext_get_dev_msg_data(struct device *pdev){
    struct usb_ucext *dev; 
    if(!pdev) {
        return 0;
    }
    dev = dev_get_drvdata(pdev);

    return dev->p_message_data;
}

void *ucext_get_dev_tivac_data(struct device *pdev){
    struct usb_ucext *dev; 
    if(!pdev) {
        return 0;
    }
    dev = dev_get_drvdata(pdev);

    return dev->p_dev_tivac_data;
}

void *ucext_get_dev_sharedpwr_data(struct device *pdev){
    struct usb_ucext *dev; 
    if(!pdev) {
        return 0;
    }
    dev = dev_get_drvdata(pdev);

    return dev->p_dev_sharedpwr_data;
}


void *ucext_get_dev_tomahawk_data(struct device *pdev){
    struct usb_ucext *dev; 
    if(!pdev) {
        return 0;
    }
    dev = dev_get_drvdata(pdev);

    return dev->p_dev_tomahawk_data;
}

void *ucext_get_dev_denverton_data(struct device *pdev){
    struct usb_ucext *dev; 
    if(!pdev) {
        return 0;
    }
    dev = dev_get_drvdata(pdev);

    return dev->p_dev_denverton_data;
}

int ucext_get_dev_msg_debug(struct device *pdev){
    struct usb_ucext *dev; 
    if(!pdev) {
        return 0;
    }
    dev = dev_get_drvdata(pdev);

    return dev->msg_debug;
}

int ucext_set_dev_msg_debug(struct device *pdev, int level){
    struct usb_ucext *dev; 
    if(!pdev) {
        return -EINVAL;
    }
    dev = dev_get_drvdata(pdev);

    if(dev) {
        dev->msg_debug = level;
        return 0;
    }
    else
    {
        return -ENODEV;
    }
}

/* This function uses data buffers allocated by calling module, not the isolated file specific ones */
int ucext_cmd_response(struct device *pdev, int cmd_len, unsigned char *cmd_buff, 
                       unsigned char *rsp_buff, int *rsp_len, int timeout_msec )
{
    int retval = 0;
    int bulk_wrote_cnt = 0;
    struct usb_ucext *dev = dev_get_drvdata(pdev);
    long int bulk_timeout;

    if( (dev==NULL) || (cmd_buff==NULL) || (cmd_len==0) || (rsp_buff==NULL) || (rsp_len==NULL) ) {
        dev_err(&dev->interface->dev, "ucext_cmd_response USB usage arguement error" );
        retval=-EINVAL;
    }

    /*  Init the response length to 0 for the fail cases*/
    *rsp_len = 0;

    /*  Limit the timeout range */
    bulk_timeout = (timeout_msec*HZ)/1000;
    if(bulk_timeout<=0) {
        bulk_timeout = 1;
    }
    else if(bulk_timeout>INT_MAX) {
        bulk_timeout = INT_MAX;
    }

    mutex_lock(&dev->io_mutex);
    if(!dev->interface) {      /* disconnect() was called */
        retval = -ENODEV;
        goto error;
    }

    /* Write the message into the USB bulk out endpoint */
    retval = usb_bulk_msg(dev->udev, usb_sndbulkpipe(dev->udev, dev->bulk_out_endpointAddr),
                          cmd_buff, MIN(cmd_len, (int)dev->bulk_in_size), &bulk_wrote_cnt, bulk_timeout);

    if(retval ) {
        dev_err(&dev->interface->dev,
                 "ucext_cmd_response USB TX error to ucext%d returned %d failure",
                 dev->interface->minor, retval);
        goto error;
    }
    else {
        if(dev->msg_debug) {
            dev_info(&dev->interface->dev,
                     "ucext_cmd_response USB TX to ucext%d sent Hdr(0x%02x,0x%02x,0x%02x,0x%02x) Len=%d",
                     dev->interface->minor,cmd_buff[0],cmd_buff[1],cmd_buff[2],cmd_buff[3],bulk_wrote_cnt);
        }
    }

    /* Read the message from the USB bulk in endpoint */
    retval = usb_bulk_msg(dev->udev, usb_rcvbulkpipe(dev->udev, dev->bulk_in_endpointAddr),
                          rsp_buff, (int)dev->bulk_in_size, rsp_len, bulk_timeout);

    if(retval) {
        dev_err(&dev->interface->dev,
                 "ucext_cmd_response USB RX from ucext%d returned %d failure",
                 dev->interface->minor, retval);
        goto error;
    }

    if(dev->msg_debug) {
        dev_info(&dev->interface->dev,
                 "ucext_cmd_response USB RX from ucext%d was Hdr(0x%02x,0x%02x,0x%02x,0x%02x ) Len=%d",
                 dev->interface->minor, rsp_buff[0],rsp_buff[1],rsp_buff[2],rsp_buff[3], *rsp_len);
    }

    error:
    mutex_unlock(&dev->io_mutex);
    return retval;
}

static void ucext_delete(struct kref *kref)
{
    struct usb_ucext *dev = to_ucext_dev(kref);

    usb_free_urb(dev->bulk_in_urb);
    usb_put_dev(dev->udev);
    kfree(dev->bulk_in_buffer);
    kfree(dev);
}

static int ucext_open(struct inode *inode, struct file *file)
{
    struct usb_ucext *dev;
    struct usb_ucext_file_data *fdata;
    struct usb_interface *interface;
    int subminor;
    int retval = 0;

    subminor = iminor(inode);

    interface = usb_find_interface(&ucext_driver, subminor);
    if(!interface) {
        pr_err("%s - error, can't find device for minor %d\n",
               __func__, subminor);
        retval = -ENODEV;
        goto exit;
    }

    dev = usb_get_intfdata(interface);
    if(!dev) {
        retval = -ENODEV;
        goto exit;
    }

    retval = usb_autopm_get_interface(interface);
    if(retval)
        goto exit;

    fdata = kmalloc(sizeof(*fdata), GFP_KERNEL);
    if(!fdata) {
        retval = -ENOMEM;
        goto exit;
    }
       
    /* increment our usage count for the device */
    kref_get(&dev->kref);

    if(dev->msg_debug) {
        dev_info(&dev->interface->dev,
                 "ucext_open of ucext%d file by %s, pid %d\n",
                 dev->interface->minor, current->comm, (int)task_pid_nr(current));
    }

    /* save dev pointer in per file private data */
    fdata->ucextdev = dev;
    fdata->bulk_rd_cnt = 0;
    fdata->bulk_wr_cnt = 0;
    fdata->bulk_rd_ready_cnt = 0;

        /* save our object in the file's private structure */
    file->private_data = fdata;

    exit:
    return retval;
}

static int ucext_release(struct inode *inode, struct file *file)
{
    struct usb_ucext *dev;
    struct usb_ucext_file_data *fdata;

    fdata = file->private_data;
    if(fdata == NULL) {
        return -ENODEV;
    }
    else {
       dev = fdata->ucextdev;
    }

    /* Null device pointer just incase someone in user space still has handle so they won't corrupt */
    fdata->ucextdev = NULL;
    fdata->bulk_rd_cnt = 0;
    fdata->bulk_wr_cnt = 0;
    fdata->bulk_rd_ready_cnt = 0;

    /* Now free the per file allocated data buffers */
    kfree(fdata);

    /* Now act on dev now that per file data cleaned up */
    if(dev == NULL)
        return -ENODEV;

    if(dev->msg_debug) {
        dev_info(&dev->interface->dev,
                 "ucext_release of ucext%d file by %s, pid %d\n",
                 dev->interface->minor, current->comm, (int)task_pid_nr(current));
    }

    /* allow the device to be autosuspended */
    mutex_lock(&dev->io_mutex);
    if(dev->interface)
        usb_autopm_put_interface(dev->interface);
    mutex_unlock(&dev->io_mutex);

    /* decrement the count on our device */
    kref_put(&dev->kref, ucext_delete);
    return 0;
}

static int ucext_flush(struct file *file, fl_owner_t id)
{
    int res;
    struct usb_ucext *dev;
    struct usb_ucext_file_data *fdata;

    fdata = file->private_data;
    if(fdata == NULL)
        return -ENODEV;

    dev = fdata->ucextdev;
    if(dev == NULL)
        return -ENODEV;

    /* wait for io to stop */
    mutex_lock(&dev->io_mutex);
    ucext_draw_down(dev);

    /* read out errors, leave subsequent opens a clean slate */
    spin_lock_irq(&dev->err_lock);
    res = dev->errors ? (dev->errors == -EPIPE ? -EPIPE : -EIO) : 0;
    dev->errors = 0;
    spin_unlock_irq(&dev->err_lock);

    mutex_unlock(&dev->io_mutex);

    return res;
}

#if (!defined(UCEXT_FOP_READ_HAS_TIMEOUT) || (UCEXT_FOP_READ_HAS_TIMEOUT==0))   
/* These are the high speed BULK USB URB read functions (ie no timeouts) */

static void ucext_read_bulk_callback(struct urb *urb)
{
    struct usb_ucext *dev;
#if LINUX_VERSION_CODE > KERNEL_VERSION(4,9,47)
    unsigned long flags;
#endif

    dev = urb->context;

#if LINUX_VERSION_CODE > KERNEL_VERSION(4,9,47)
    spin_lock_irqsave(&dev->err_lock, flags);
#else
    spin_lock(&dev->err_lock);
#endif

    /* sync/async unlink faults aren't errors */
    if(urb->status) {
        if(!(urb->status == -ENOENT ||
             urb->status == -ECONNRESET ||
             urb->status == -ESHUTDOWN))
            dev_err(&dev->interface->dev,
                    "%s - nonzero write bulk status received: %d\n",
                    __func__, urb->status);

        dev->errors = urb->status;
    }
    else {
        dev->bulk_in_filled = urb->actual_length;
    }
    dev->ongoing_read = 0;
#if LINUX_VERSION_CODE > KERNEL_VERSION(4,9,47)
    spin_unlock_irqrestore(&dev->err_lock, flags);
#else
    spin_unlock(&dev->err_lock);
#endif

    wake_up_interruptible(&dev->bulk_in_wait);
}

static int ucext_do_read_io(struct usb_ucext *dev, size_t count)
{
    int rv;

    /* prepare a read */
    usb_fill_bulk_urb(dev->bulk_in_urb,
                      dev->udev,
                      usb_rcvbulkpipe(dev->udev,
                                      dev->bulk_in_endpointAddr),
                      dev->bulk_in_buffer,
                      min(dev->bulk_in_size, count),
                      ucext_read_bulk_callback,
                      dev);
    /* tell everybody to leave the URB alone */
    spin_lock_irq(&dev->err_lock);
    dev->ongoing_read = 1;
    spin_unlock_irq(&dev->err_lock);

    /* submit bulk in urb, which means no data to deliver */
    dev->bulk_in_filled = 0;
    dev->bulk_in_copied = 0;

    /* do it */
    rv = usb_submit_urb(dev->bulk_in_urb, GFP_KERNEL);
    if(rv < 0) {
        dev_err(&dev->interface->dev,
                "%s - failed submitting read urb, error %d\n",
                __func__, rv);
        rv = (rv == -ENOMEM) ? rv : -EIO;
        spin_lock_irq(&dev->err_lock);
        dev->ongoing_read = 0;
        spin_unlock_irq(&dev->err_lock);
    }

    return rv;
}


static ssize_t ucext_read(struct file *file, char *buffer, size_t count,
                          loff_t *ppos)
{
    struct usb_ucext *dev;
    int rv;
    bool ongoing_io;

    dev = file->private_data;

    /* if we cannot read at all, return EOF */
    if(!dev->bulk_in_urb || !count)
        return 0;

    /* no concurrent readers */
    rv = mutex_lock_interruptible(&dev->io_mutex);
    if(rv < 0)
        return rv;

    if(!dev->interface) {      /* disconnect() was called */
        rv = -ENODEV;
        goto exit;
    }

    /* if IO is under way, we must not touch things */
    retry:
    spin_lock_irq(&dev->err_lock);
    ongoing_io = dev->ongoing_read;
    spin_unlock_irq(&dev->err_lock);

    if(ongoing_io) {
        /* nonblocking IO shall not wait */
        if(file->f_flags & O_NONBLOCK) {
            rv = -EAGAIN;
            goto exit;
        }
        /*
         * IO may take forever
         * hence wait in an interruptible state
         */
        rv = wait_event_interruptible(dev->bulk_in_wait, (!dev->ongoing_read));
        if(rv < 0)
            goto exit;
    }

    /* errors must be reported */
    rv = dev->errors;
    if(rv < 0) {
        /* any error is reported once */
        dev->errors = 0;
        /* to preserve notifications about reset */
        rv = (rv == -EPIPE) ? rv : -EIO;
        /* report it */
        goto exit;
    }

    /*
     * if the buffer is filled we may satisfy the read
     * else we need to start IO
     */

    if(dev->bulk_in_filled) {
        /* we had read data */
        size_t available = dev->bulk_in_filled - dev->bulk_in_copied;
        size_t chunk = min(available, count);

        if(!available) {
            /*
             * all data has been used
             * actual IO needs to be done
             */
            rv = ucext_do_read_io(dev, count);
            if(rv < 0)
                goto exit;
            else
                goto retry;
        }
        /*
         * data is available
         * chunk tells us how much shall be copied
         */

        if(copy_to_user(buffer,
                        dev->bulk_in_buffer + dev->bulk_in_copied,
                        chunk))
            rv = -EFAULT;
        else
            rv = chunk;

        dev->bulk_in_copied += chunk;

        /*
         * if we are asked for more than we have,
         * we start IO but don't wait
         */
        if(available < count)
            ucext_do_read_io(dev, count - chunk);
    }
    else {
        /* no data in the buffer */
        rv = ucext_do_read_io(dev, count);
        if(rv < 0)
            goto exit;
        else
            goto retry;
    }
    exit:
    mutex_unlock(&dev->io_mutex);
    return rv;
}

#else

/* These are the lower speed usb_bulk_msg read functions blocking writes that include timeouts */
static ssize_t ucext_read(struct file *file, char *user_buffer, size_t count,
                          loff_t *ppos)
{
    int retval = 0;
    int read_cnt = 0;
    long int bulk_timeout;
    struct usb_ucext *dev;
    struct usb_ucext_file_data *fdata;

    if(!file||!user_buffer) {
        return -EINVAL;
    }

    if(file->f_flags & O_NONBLOCK) {
        return -EOPNOTSUPP;
    }

    fdata = file->private_data;
    if(fdata == NULL)
        return -ENODEV;

    dev = fdata->ucextdev;
    if(dev == NULL)
        return -ENODEV;

    // Set the max read size
    fdata->bulk_rd_cnt = MIN(count, dev->bulk_in_size);

    /*  Set the timeout */
    bulk_timeout = ((UCEXT_MSG_TIMEOUT_MSEC)*HZ)/1000;

    mutex_lock(&dev->io_mutex);
    if(!dev->interface) {      /* disconnect() was called */
        retval = -ENODEV;
        goto error;
    }

    /* Skip processing when count was 0 */
    if(fdata->bulk_rd_cnt) {
        /* See buffered RX packets already waiting from prior async TX */
        if(fdata->bulk_rd_ready_cnt<=0)
        {
            /* Read the message from the USB bulk in endpoint (skip read when 0) */
            retval = usb_bulk_msg(dev->udev, usb_rcvbulkpipe(dev->udev, dev->bulk_in_endpointAddr),
                                  fdata->bulk_rd_buff, fdata->bulk_rd_cnt, &read_cnt, bulk_timeout);
            if(retval) {
                dev_err(&dev->interface->dev,
                         "ucext_read usb_bulk_msg RX from ucext%d returned %d failure",
                         dev->interface->minor, retval);
                goto error;
            }
            if(dev->msg_debug) {
                dev_info(&dev->interface->dev,
                         "ucext_read usb_bulk_msg RX from ucext%d was Hdr(0x%02x,0x%02x,0x%02x,0x%02x ) Len=%d",
                         dev->interface->minor, fdata->bulk_rd_buff[0],fdata->bulk_rd_buff[1],fdata->bulk_rd_buff[2],fdata->bulk_rd_buff[3], read_cnt);
            }
        }
        else
        {
            /* Set count to read the pending data buff instead and now clear the pending data count */
            /*  for next TX or RX event before unlocking Mutex */
            read_cnt = fdata->bulk_rd_ready_cnt;    
            fdata->bulk_rd_ready_cnt = 0;
            if(dev->msg_debug) {
                dev_info(&dev->interface->dev,
                         "ucext_read buffered RX from ucext%d was Hdr(0x%02x,0x%02x,0x%02x,0x%02x ) Len=%d",
                         dev->interface->minor, fdata->bulk_rd_buff[0],fdata->bulk_rd_buff[1],fdata->bulk_rd_buff[2],fdata->bulk_rd_buff[3], read_cnt);
            }
        }
    }
            
    /* Make sure returned small enough for user space before copy just in case there's a bug*/
    read_cnt = MIN(count,read_cnt);

    if( copy_to_user(user_buffer, fdata->bulk_rd_buff, read_cnt) ) {
        retval = -EFAULT;
    }
    else {
        retval = read_cnt;
    }

    error:
    mutex_unlock(&dev->io_mutex);
    return retval;
}

#endif

#if (!defined(UCEXT_FOP_WRITE_HAS_TIMEOUT) || (UCEXT_FOP_WRITE_HAS_TIMEOUT==0))   
/* These are the high speed BULK USB URB write functions (ie no timeouts) */

static void ucext_write_bulk_callback(struct urb *urb)
{
    struct usb_ucext *dev;
#if LINUX_VERSION_CODE > KERNEL_VERSION(4,9,47)
    unsigned long flags;
#endif

    dev = urb->context;

    /* sync/async unlink faults aren't errors */
    if(urb->status) {
        if(!(urb->status == -ENOENT ||
             urb->status == -ECONNRESET ||
             urb->status == -ESHUTDOWN))
            dev_err(&dev->interface->dev,
                    "%s - nonzero write bulk status received: %d\n",
                    __func__, urb->status);

#if LINUX_VERSION_CODE > KERNEL_VERSION(4,9,47)
        spin_lock_irqsave(&dev->err_lock, flags);
#else
        spin_lock(&dev->err_lock);
#endif
        dev->errors = urb->status;
#if LINUX_VERSION_CODE > KERNEL_VERSION(4,9,47)
        spin_unlock_irqrestore(&dev->err_lock, flags);
#else
        spin_unlock(&dev->err_lock);
#endif
    }

    /* free up our allocated buffer */
    usb_free_coherent(urb->dev, urb->transfer_buffer_length,
                      urb->transfer_buffer, urb->transfer_dma);
    up(&dev->limit_sem);
}

static ssize_t ucext_write(struct file *file, const char *user_buffer,
                           size_t count, loff_t *ppos)
{
    struct usb_ucext *dev;
    int retval = 0;
    struct urb *urb = NULL;
    char *buf = NULL;
    size_t writesize = min(count, (size_t)MAX_TRANSFER);

    dev = file->private_data;

    /* verify that we actually have some data to write */
    if(count == 0) 
        goto exit;

    /*
     * limit the number of URBs in flight to stop a user from using up all
     * RAM
     */
    if(!(file->f_flags & O_NONBLOCK)) {
        if(down_interruptible(&dev->limit_sem)) {
            retval = -ERESTARTSYS;
            goto exit;
        }
    }
    else {
        if(down_trylock(&dev->limit_sem)) {
            retval = -EAGAIN;
            goto exit;
        }
    }

    spin_lock_irq(&dev->err_lock);
    retval = dev->errors;
    if(retval < 0) {
        /* any error is reported once */
        dev->errors = 0;
        /* to preserve notifications about reset */
        retval = (retval == -EPIPE) ? retval : -EIO;
    }
    spin_unlock_irq(&dev->err_lock);
    if(retval < 0)
        goto error;

    /* create a urb, and a buffer for it, and copy the data to the urb */
    urb = usb_alloc_urb(0, GFP_KERNEL);
    if(!urb) {
        retval = -ENOMEM;
        goto error;
    }

    buf = usb_alloc_coherent(dev->udev, writesize, GFP_KERNEL,
                             &urb->transfer_dma);
    if(!buf) {
        retval = -ENOMEM;
        goto error;
    }

    /* this lock makes sure we don't submit URBs to gone devices */
    mutex_lock(&dev->io_mutex);
    if(!dev->interface) {      /* disconnect() was called */
        mutex_unlock(&dev->io_mutex);
        retval = -ENODEV;
        goto error;
    }

    if(copy_from_user(buf, user_buffer, writesize)) {
        retval = -EFAULT;
        goto error;
    }

    /* initialize the urb properly */
    usb_fill_bulk_urb(urb, dev->udev,
                      usb_sndbulkpipe(dev->udev, dev->bulk_out_endpointAddr),
                      buf, writesize, ucext_write_bulk_callback, dev);
    urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
    usb_anchor_urb(urb, &dev->submitted);

    /* send the data out the bulk port */
    retval = usb_submit_urb(urb, GFP_KERNEL);
    mutex_unlock(&dev->io_mutex);
    if(retval) {
        dev_err(&dev->interface->dev,
                "%s - failed submitting write urb, error %d\n",
                __func__, retval);
        goto error_unanchor;
    }

    /*
     * release our reference to this urb, the USB core will eventually free
     * it entirely
     */
    usb_free_urb(urb);


    return writesize;

    error_unanchor:
    usb_unanchor_urb(urb);
    error:
    if(urb) {
        usb_free_coherent(dev->udev, writesize, buf, urb->transfer_dma);
        usb_free_urb(urb);
    }
    up(&dev->limit_sem);

    exit:
    return retval;
}

#else

/* These are the lower speed usb_bulk_msg write functions blocking writes that include timeouts */
static ssize_t ucext_write(struct file *file, const char *user_buffer,
                           size_t count, loff_t *ppos)
{
    int retval = 0;
    int wrote_cnt = 0;
    long int bulk_timeout;
    int retread = 0;
    int read_cnt = 0;

    struct usb_ucext *dev;
    struct usb_ucext_file_data *fdata;

    if(!file||!user_buffer) {
        return -EINVAL;
    }

    if(file->f_flags & O_NONBLOCK) {
        return -EOPNOTSUPP;
    }

    fdata = file->private_data;
    if(fdata == NULL)
        return -ENODEV;

    dev = fdata->ucextdev;
    if(dev == NULL)
        return -ENODEV;

    if(count==0) {
        return 0;
    }

    mutex_lock(&dev->io_mutex);
    if(!dev->interface) {      /* disconnect() was called */
        retval = -EACCES;
        goto error;
    }
    // Set the max write size
    fdata->bulk_wr_cnt = MIN(count, dev->bulk_in_size);
//    dev->bulk_wr_cnt = MIN(count, MAX_TRANSFER);

    if( copy_from_user(fdata->bulk_wr_buff, user_buffer, fdata->bulk_wr_cnt) )
    {
        retval = -EINVAL;
        goto error;
    }

    // When Transmit to be issued and already locked, always start out with read ready count zero
    fdata->bulk_rd_ready_cnt = 0;

    /*  Set the timeout */
    bulk_timeout = ((UCEXT_MSG_TIMEOUT_MSEC)*HZ)/1000;

   /* Write the message into the USB bulk out endpoint */
    retval = usb_bulk_msg(dev->udev, usb_sndbulkpipe(dev->udev, dev->bulk_out_endpointAddr),
                          fdata->bulk_wr_buff, fdata->bulk_wr_cnt, &wrote_cnt, bulk_timeout);
    if(retval ) {
        dev_err(&dev->interface->dev,
                 "ucext_write usb_bulk_msg TX error to ucext%d returned %d failure",
                 dev->interface->minor, retval);
        //retval = -EFAULT;
    }
    else {
        if(dev->msg_debug) {
            dev_info(&dev->interface->dev,
                     "ucext_write USB TX to ucext%d sent Hdr(0x%02x,0x%02x,0x%02x,0x%02x) Len=%d",
                     dev->interface->minor,fdata->bulk_wr_buff[0],fdata->bulk_wr_buff[1],fdata->bulk_wr_buff[2],
                     fdata->bulk_wr_buff[3], wrote_cnt);
        }
        retval = wrote_cnt;
    }

    /* While still locked, try to read response and buffer it for possible later async RX */

    // Set the max read size allowed by buffer
    fdata->bulk_rd_cnt = MAX_TRANSFER;
    
    /* Read the message from the USB bulk in endpoint */
    retread = usb_bulk_msg(dev->udev, usb_rcvbulkpipe(dev->udev, dev->bulk_in_endpointAddr),
                              fdata->bulk_rd_buff, fdata->bulk_rd_cnt, &read_cnt, bulk_timeout);

    if(retread) {
        dev_err(&dev->interface->dev,
                 "ucext_write usb_bulk_msg resp from ucext%d returned %d failure",
                 dev->interface->minor, retval);
        goto error;
    }

    if(dev->msg_debug) {
        dev_info(&dev->interface->dev,
                 "ucext_write USB TX response from ucext%d was Hdr(0x%02x,0x%02x,0x%02x,0x%02x ) Len=%d",
                 dev->interface->minor, fdata->bulk_rd_buff[0],fdata->bulk_rd_buff[1],fdata->bulk_rd_buff[2],fdata->bulk_rd_buff[3], read_cnt);
    }

    /* When  received response, set the read ready count to indicate buffered RX data ready to */
    /* the async read routine before unlocking */
    if(read_cnt) {
        fdata->bulk_rd_ready_cnt = read_cnt;
    }

    error:
    mutex_unlock(&dev->io_mutex);
    return retval;
}
#endif


static const struct file_operations ucext_fops = {
    .owner =    THIS_MODULE,
    .read =     ucext_read,
    .write =    ucext_write,
    .open =     ucext_open,
    .release =  ucext_release,
    .flush =    ucext_flush,
    .llseek =   noop_llseek,
};

/*
 * usb class driver info in order to get a minor number from the usb core,
 * and to have the device registered with the driver core
 */
static struct usb_class_driver ucext_class = {
    .name =     "ucext%d",
    .fops =     &ucext_fops,
    .minor_base =   UCEXT_USB_MINOR_BASE,
};


static int ucext_probe(struct usb_interface *interface,
                       const struct usb_device_id *id)
{
    struct usb_ucext *dev;
    int retval = 0;

#if LINUX_VERSION_CODE > KERNEL_VERSION(4,9,47)
    struct usb_endpoint_descriptor *bulk_in, *bulk_out;
#else
    int i;  
    struct usb_host_interface *iface_desc;
    struct usb_endpoint_descriptor *endpoint;
    size_t buffer_size;
#endif
    /* User Mode Helper Script to run if it exists */
    static char *argv[] = { "/bin/bash", "-c", "/etc/init.d/board_sensor_profile.sh", NULL};
    static char *envp[] = { "HOME=/",
                            "TERM=linux",
                            "PATH=/sbin:/bin:/usr/sbin:/usr/bin", NULL };

    /* allocate memory for our device state and initialize it */
    dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if(!dev)
        return -ENOMEM;

    dev->msg_debug = UCEXT_DEBUG_MESSAGE_LEVEL;
    kref_init(&dev->kref);
    sema_init(&dev->limit_sem, WRITES_IN_FLIGHT);
    mutex_init(&dev->io_mutex);
    spin_lock_init(&dev->err_lock);
    init_usb_anchor(&dev->submitted);
    init_waitqueue_head(&dev->bulk_in_wait);

    dev->udev = usb_get_dev(interface_to_usbdev(interface));
    dev->interface = interface;

    /* set up the endpoint information */
    /* use only the first bulk-in and bulk-out endpoints */
#if LINUX_VERSION_CODE > KERNEL_VERSION(4,9,47)
    retval = usb_find_common_endpoints(interface->cur_altsetting,
                                       &bulk_in, &bulk_out, NULL, NULL);
    if(retval) {
        dev_err(&interface->dev,
                "Could not find both bulk-in and bulk-out endpoints\n");
        goto error;
    }

    dev->bulk_in_size = usb_endpoint_maxp(bulk_in);
    dev->bulk_in_endpointAddr = bulk_in->bEndpointAddress;
    dev->bulk_in_buffer = kmalloc(dev->bulk_in_size, GFP_KERNEL);
    if(!dev->bulk_in_buffer) {
        retval = -ENOMEM;
        goto error;
    }
    dev->bulk_in_urb = usb_alloc_urb(0, GFP_KERNEL);
    if(!dev->bulk_in_urb) {
        retval = -ENOMEM;
        goto error;
    }
    dev->bulk_out_endpointAddr = bulk_out->bEndpointAddress;
#else
    iface_desc = interface->cur_altsetting;
    for(i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
        endpoint = &iface_desc->endpoint[i].desc;

        if(!dev->bulk_in_endpointAddr &&
           usb_endpoint_is_bulk_in(endpoint)) {
            /* we found a bulk in endpoint */
            buffer_size = usb_endpoint_maxp(endpoint);
            dev->bulk_in_size = buffer_size;
            dev->bulk_in_endpointAddr = endpoint->bEndpointAddress;
            dev->bulk_in_buffer = kmalloc(buffer_size, GFP_KERNEL);
            if(!dev->bulk_in_buffer)
                goto error;
            dev->bulk_in_urb = usb_alloc_urb(0, GFP_KERNEL);
            if(!dev->bulk_in_urb)
                goto error;
        }

        if(!dev->bulk_out_endpointAddr &&
           usb_endpoint_is_bulk_out(endpoint)) {
            /* we found a bulk out endpoint */
            dev->bulk_out_endpointAddr = endpoint->bEndpointAddress;
        }
    }
    if(!(dev->bulk_in_endpointAddr && dev->bulk_out_endpointAddr)) {
        dev_err(&interface->dev,
                "Could not find both bulk-in and bulk-out endpoints\n");
        goto error;
    }
#endif

    /* save our data pointer in this interface device */
    usb_set_intfdata(interface, dev);

    /* we can register the device now, as it is ready */
    retval = usb_register_dev(interface, &ucext_class);
    if(retval) {
        /* something prevented us from registering this driver */
        dev_err(&interface->dev,
                "Not able to get a minor for this device.\n");
        usb_set_intfdata(interface, NULL);
        goto error;
    }

    /* let the user know what node this device is now attached to */
    dev_info(&interface->dev,
             "uController Extend USB driver ver %u.%u attached to device ucext%d (in_size=%d, max=%d)", 
             ((unsigned int)UCEXT_MSG_BIN_REVISION_EXPECTED)>>16, ((unsigned int)UCEXT_MSG_BIN_REVISION_EXPECTED)&0x0ffff,

             interface->minor, (int)dev->bulk_in_size, (int)MAX_TRANSFER);

    // Init the UCEXT USB Message Interface
    dev->p_message_data = ucext_msg_init(&interface->dev);

    // Init the TIVAC Multi-Function Device Message Interface and auxillary devices
    dev->p_dev_tivac_data = ucext_dev_tivac_init(&interface->dev);

    // Init the TIVAC Multi-Function Device Message Interface to auxillary devices
    dev->p_dev_sharedpwr_data = ucext_dev_sharedpwr_init(&interface->dev);
    dev->p_dev_tomahawk_data = ucext_dev_tomahawk_init(&interface->dev);
    dev->p_dev_denverton_data = ucext_dev_denverton_init(&interface->dev);

    // Run the bash board_sensor_profile.sh using usermodehelper process 
    // to configure HWMON attributes (Labels, Alaram Thresholds)
    dev_info(&interface->dev, "uController Extend USB ucext%d running %s script", interface->minor, argv[2]);
    call_usermodehelper(argv[0], argv, envp, UMH_NO_WAIT);

    return 0;

    error:
    /* this frees allocated memory */
    kref_put(&dev->kref, ucext_delete);

    return retval;
}

static void ucext_disconnect(struct usb_interface *interface)
{
    struct usb_ucext *dev;
    int minor = interface->minor;

    dev = usb_get_intfdata(interface);

    /* Return MSP430 denverton subsystem private data structures*/
    ucext_dev_denverton_exit(&interface->dev);
    dev->p_dev_denverton_data=NULL;

    /* Return MSP430 tomahawk subsystem private data structures*/
    ucext_dev_tomahawk_exit(&interface->dev);
    dev->p_dev_tomahawk_data=NULL;

    /* Return MSP430 sharedpwr subsystem private data structures*/
    ucext_dev_sharedpwr_exit(&interface->dev);
    dev->p_dev_sharedpwr_data=NULL;

    /* Return TIVAC subsystem private data structures*/
    ucext_dev_tivac_exit(&interface->dev);
    dev->p_dev_tivac_data=NULL;

    /* Return MSG subsystem private data structures*/
    ucext_msg_exit(&interface->dev);
    dev->p_message_data=NULL;

    /* give back our minor */
    usb_deregister_dev(interface, &ucext_class);
    usb_set_intfdata(interface, NULL);

    /* prevent more I/O from starting */
    mutex_lock(&dev->io_mutex);
    dev->interface = NULL;
    mutex_unlock(&dev->io_mutex);

    usb_kill_anchored_urbs(&dev->submitted);

    /* decrement our usage count */
    kref_put(&dev->kref, ucext_delete);

    dev_info(&interface->dev, "uController Extend USB device ucext#%d now disconnected", minor);
}

static void ucext_draw_down(struct usb_ucext *dev)
{
    int time;

    time = usb_wait_anchor_empty_timeout(&dev->submitted, 1000);
    if(!time)
        usb_kill_anchored_urbs(&dev->submitted);
    usb_kill_urb(dev->bulk_in_urb);
}

static int ucext_suspend(struct usb_interface *intf, pm_message_t message)
{
    struct usb_ucext *dev = usb_get_intfdata(intf);

    if(!dev)
        return 0;
    ucext_draw_down(dev);
    return 0;
}

static int ucext_resume(struct usb_interface *intf)
{
    return 0;
}

static int ucext_pre_reset(struct usb_interface *intf)
{
    struct usb_ucext *dev = usb_get_intfdata(intf);

    mutex_lock(&dev->io_mutex);
    ucext_draw_down(dev);

    return 0;
}

static int ucext_post_reset(struct usb_interface *intf)
{
    struct usb_ucext *dev = usb_get_intfdata(intf);

    /* we are sure no URBs are active - no locking needed */
    dev->errors = -EPIPE;
    mutex_unlock(&dev->io_mutex);

    return 0;
}

static struct usb_driver ucext_driver = {
    .name =     "ucextusb",
    .probe =    ucext_probe,
    .disconnect =  ucext_disconnect,
    .suspend =  ucext_suspend,
    .resume =   ucext_resume,
    .pre_reset =    ucext_pre_reset,
    .post_reset =   ucext_post_reset,
    .id_table = ucext_table,
    .supports_autosuspend = 1,
};

module_usb_driver(ucext_driver);

MODULE_DESCRIPTION("UCEXT USB TIVAC-C driver");
MODULE_LICENSE("GPL v2");

