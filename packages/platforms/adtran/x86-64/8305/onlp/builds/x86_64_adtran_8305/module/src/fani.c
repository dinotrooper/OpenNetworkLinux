/************************************************************
 * <bsn.cl fy=2014 v=onl>
 *
 *           Copyright 2014 Big Switch Networks, Inc.
 *           Copyright 2014 Accton Technology Corporation.
 *
 * Licensed under the Eclipse Public License, Version 1.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *        http://www.eclipse.org/legal/epl-v10.html
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the
 * License.
 *
 * </bsn.cl>
 ************************************************************
 *
 * Fan Platform Implementation Defaults.
 *
 ***********************************************************/
#include <onlp/platformi/fani.h>
#include <onlplib/mmap.h>
#include <fcntl.h>
#include <limits.h>
#include "platform_lib.h"

#define PREFIX_PATH_ON_MAIN_BOARD   "/sys/class/hwmon/hwmon2/"

#define MAX_FAN_PWM     255

#define PROJECT_NAME
#define LEN_FILE_NAME 80

#define FAN_RESERVED        0
#define FAN_1_ON_MAIN_BOARD 1
#define FAN_2_ON_MAIN_BOARD	2
#define FAN_3_ON_MAIN_BOARD	3
#define FAN_4_ON_MAIN_BOARD	4


typedef struct fan_path_S
{
    char fault[LEN_FILE_NAME];
    char alarm[LEN_FILE_NAME];
    char speed[LEN_FILE_NAME];
    char label[LEN_FILE_NAME];
    char pwm[LEN_FILE_NAME];
}fan_path_T;

#define _MAKE_FAN_PATH_ON_MAIN_BOARD(prj,id) \
    { #prj"fan"#id"_fault", #prj"fan"#id"_alarm", #prj"fan"#id"_input", \
      #prj"fan"#id"_label", #prj"pwm1"}

#define MAKE_FAN_PATH_ON_MAIN_BOARD(prj,id) _MAKE_FAN_PATH_ON_MAIN_BOARD(prj,id)

static fan_path_T fan_path[] =  /* must map with onlp_fan_id */
{
    MAKE_FAN_PATH_ON_MAIN_BOARD(PROJECT_NAME, FAN_RESERVED),
    MAKE_FAN_PATH_ON_MAIN_BOARD(PROJECT_NAME, FAN_1_ON_MAIN_BOARD),
    MAKE_FAN_PATH_ON_MAIN_BOARD(PROJECT_NAME, FAN_2_ON_MAIN_BOARD),
    MAKE_FAN_PATH_ON_MAIN_BOARD(PROJECT_NAME, FAN_3_ON_MAIN_BOARD),
    MAKE_FAN_PATH_ON_MAIN_BOARD(PROJECT_NAME, FAN_4_ON_MAIN_BOARD),
};

#define MAKE_FAN_INFO_NODE_ON_MAIN_BOARD(id) \
    { \
        { ONLP_FAN_ID_CREATE(FAN_##id##_ON_MAIN_BOARD), "FAN #"#id, 0 }, \
        0x0, \
        (ONLP_FAN_CAPS_SET_PERCENTAGE | ONLP_FAN_CAPS_GET_RPM | ONLP_FAN_CAPS_GET_PERCENTAGE), \
        0, \
        0, \
        ONLP_FAN_MODE_INVALID, \
    }

/* Static fan information */
onlp_fan_info_t linfo[] = {
    { }, /* Not used */
    MAKE_FAN_INFO_NODE_ON_MAIN_BOARD(1),
    MAKE_FAN_INFO_NODE_ON_MAIN_BOARD(2),
    MAKE_FAN_INFO_NODE_ON_MAIN_BOARD(3),
    MAKE_FAN_INFO_NODE_ON_MAIN_BOARD(4),
};

#define VALIDATE(_id)                           \
    do {                                        \
        if(!ONLP_OID_IS_FAN(_id)) {             \
            return ONLP_STATUS_E_INVALID;       \
        }                                       \
    } while(0)

#define OPEN_READ_FILE(fd,fullpath,data,nbytes,len) \
    DEBUG_PRINT("[Debug][%s][%d][openfile: %s]\n", __FUNCTION__, __LINE__, fullpath); \
    if ((fd = open(fullpath, O_RDONLY)) == -1)  \
       return ONLP_STATUS_E_INTERNAL;           \
    if ((len = read(fd, r_data, nbytes)) <= 0){ \
        close(fd);                              \
        return ONLP_STATUS_E_INTERNAL;}         \
    DEBUG_PRINT("[Debug][%s][%d][read data: %s]\n", __FUNCTION__, __LINE__, r_data); \
    if (close(fd) == -1)                        \
        return ONLP_STATUS_E_INTERNAL


static int
_onlp_fani_info_get_fan(int local_id, onlp_fan_info_t* info)
{
    int   fd, len, nbytes = 10;
    char  r_data[10]   = {0};
    char  fullpath[PATH_MAX] = {0};

    /* check if fan is present */
    sprintf(fullpath, "%s%s", PREFIX_PATH_ON_MAIN_BOARD, fan_path[local_id].fault)
    OPEN_READ_FILE(fd,fullpath,r_data,nbytes,len);
    if (atoi(r_data) == 1) {
        return ONLP_STATUS_OK;
    }
    info->status |= ONLP_FAN_STATUS_PRESENT;

    /* get fan alarm status */
    sprintf(fullpath, "%s%s", PREFIX_PATH_ON_MAIN_BOARD, fan_path[local_id].alarm);
    OPEN_READ_FILE(fd,fullpath,r_data,nbytes,len);
    if (atoi(r_data) > 0) {
        info->status |= ONLP_FAN_STATUS_FAILED;
        return ONLP_STATUS_OK;
    }

    /* get fan speed */
    sprintf(fullpath, "%s%s", PREFIX_PATH_ON_MAIN_BOARD, fan_path[local_id].speed);
    OPEN_READ_FILE(fd,fullpath,r_data,nbytes,len);
    info->rpm = atoi(r_data);

    /* get fan percentage */
    sprintf(fullpath, "%s%s", PREFIX_PATH_ON_MAIN_BOARD, fan_path[local_id].pwm)
    OPEN_READ_FILE(fd, fullpath, r_data, nbytes, len);
    info->percentage = (int)(((double)atoi(r_data)/(double)MAX_FAN_PWM)*100);

    return ONLP_STATUS_OK;
}


/*
 * This function will be called prior to all of onlp_fani_* functions.
 */
int
onlp_fani_init(void)
{
    return ONLP_STATUS_OK;
}

int
onlp_fani_info_get(onlp_oid_t id, onlp_fan_info_t* info)
{
    int rc = 0;
    int local_id;
    VALIDATE(id);

    local_id = ONLP_OID_ID_GET(id);
    *info = linfo[local_id];

    switch (local_id)
    {
        case FAN_1_ON_MAIN_BOARD:
        case FAN_2_ON_MAIN_BOARD:
        case FAN_3_ON_MAIN_BOARD:
        case FAN_4_ON_MAIN_BOARD:
            rc =_onlp_fani_info_get_fan(local_id, info);
            break;
        default:
            rc = ONLP_STATUS_E_INVALID;
            break;
    }

    return rc;
}

/*
 * This function sets the speed of the given fan in RPM.
 *
 * This function will only be called if the fan supprots the RPM_SET
 * capability.
 *
 * It is optional if you have no fans at all with this feature.
 */
int
onlp_fani_rpm_set(onlp_oid_t id, int rpm)
{
    return ONLP_STATUS_E_UNSUPPORTED;
}

/*
 * This function sets the fan speed of the given OID as a percentage.
 *
 * This will only be called if the OID has the PERCENTAGE_SET
 * capability.
 *
 * It is optional if you have no fans at all with this feature.
 */

//PWM - "sys/class/hwmon/hwmon2/pwm1"
int
onlp_fani_percentage_set(onlp_oid_t id, int p)
{
    int  fd, len, nbytes=10, local_id;
    char data[10] = {0};
    char fullpath[PATH_MAX] = {0};

    VALIDATE(id);

    local_id = ONLP_OID_ID_GET(id);

    /* reject p=0 (p=0, stop fan) */
    if (p == 0){
        return ONLP_STATUS_E_INVALID;
    }

    /* get fullpath */
    switch (local_id)
	{
        case FAN_1_ON_MAIN_BOARD:
        case FAN_2_ON_MAIN_BOARD:
        case FAN_3_ON_MAIN_BOARD:
        case FAN_4_ON_MAIN_BOARD:
            sprintf(fullpath, "%s%s", PREFIX_PATH_ON_MAIN_BOARD, fan_path[local_id].pwm);
            break;
        default:
            return ONLP_STATUS_E_INVALID;
    }



    sprintf(data, "%d", (p/100)*MAX_FAN_PWM);
    DEBUG_PRINT("[Debug][%s][%d][openfile: %s][data=%s]\n", __FUNCTION__, __LINE__, fullpath, data);

    /* Create output file descriptor */
    fd = open(fullpath, O_WRONLY,  0644);
    if (fd == -1){
        return ONLP_STATUS_E_INTERNAL;
    }

    len = write (fd, data, (ssize_t) nbytes);
    if (len != nbytes) {
        close(fd);
        return ONLP_STATUS_E_INTERNAL;
    }

    close(fd);
    return ONLP_STATUS_OK;
}


/*
 * This function sets the fan speed of the given OID as per
 * the predefined ONLP fan speed modes: off, slow, normal, fast, max.
 *
 * Interpretation of these modes is up to the platform.
 *
 */
int
onlp_fani_mode_set(onlp_oid_t id, onlp_fan_mode_t mode)
{
    return ONLP_STATUS_E_UNSUPPORTED;
}

/*
 * This function sets the fan direction of the given OID.
 *
 * This function is only relevant if the fan OID supports both direction
 * capabilities.
 *
 * This function is optional unless the functionality is available.
 */
int
onlp_fani_dir_set(onlp_oid_t id, onlp_fan_dir_t dir)
{
    return ONLP_STATUS_E_UNSUPPORTED;
}

/*
 * Generic fan ioctl. Optional.
 */
int
onlp_fani_ioctl(onlp_oid_t id, va_list vargs)
{
    return ONLP_STATUS_E_UNSUPPORTED;
}
