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
 * Thermal Sensor Platform Implementation.
 *
 ***********************************************************/
#include <unistd.h>
#include <onlplib/mmap.h>
#include <onlplib/file.h>
#include <onlp/platformi/thermali.h>
#include <fcntl.h>
#include "platform_lib.h"

#define VALIDATE(_id)                           \
    do {                                        \
        if(!ONLP_OID_IS_THERMAL(_id)) {         \
            return ONLP_STATUS_E_INVALID;       \
        }                                       \
    } while(0)

    

enum onlp_thermal_id
{
    THERMAL_RESERVED = 0,
    RIGHT_FRONT_THERMAL_1,
    RIGHT_REAR_THERMAL_2,
    FRONT_CENTER_THERMAL_3,
    TOMAHAWK_THERMAL_4,
    LEFT_FRONT_THERMAL_5,
    DENVERTON_THERMAL_6,
};


static char* tempfiles__[] =  /* must map with onlp_thermal_id */
{
    "reserved",
    "/sys/class/hwmon/hwmon2/temp1_input",
    "/sys/class/hwmon/hwmon2/temp2_input",
    "/sys/class/hwmon/hwmon2/temp3_input",
    "/sys/class/hwmon/hwmon2/temp4_input",
    "/sys/class/hwmon/hwmon2/temp5_input",
    "/sys/class/hwmon/hwmon2/temp6_input",
};




/* Static values */
static onlp_thermal_info_t linfo[] = {
	{ }, /* Not used */
	{ { ONLP_THERMAL_ID_CREATE(RIGHT_FRONT_THERMAL_1), "Right Front Thermal Sensor 1", 0},
            ONLP_THERMAL_STATUS_PRESENT,
            ONLP_THERMAL_CAPS_ALL, 0, SDX8305_THERMAL_THRESHOLD_INIT_DEFAULTS
        },
	{ { ONLP_THERMAL_ID_CREATE(RIGHT_REAR_THERMAL_2), "Right Rear Thermal Sensor 2", 0},
            ONLP_THERMAL_STATUS_PRESENT,
            ONLP_THERMAL_CAPS_ALL, 0, SDX8305_THERMAL_THRESHOLD_INIT_DEFAULTS
        },
	{ { ONLP_THERMAL_ID_CREATE(FRONT_CENTER_THERMAL_3), "Front Center Thermal Sensor 3", 0},
            ONLP_THERMAL_STATUS_PRESENT,
            ONLP_THERMAL_CAPS_ALL, 0, SDX8305_THERMAL_THRESHOLD_INIT_DEFAULTS
        },
	{ { ONLP_THERMAL_ID_CREATE(TOMAHAWK_THERMAL_4), "Tomahawk Thermal Sensor 4", 0},
            ONLP_THERMAL_STATUS_PRESENT,
            ONLP_THERMAL_CAPS_ALL, 0, SDX8305_THERMAL_THRESHOLD_INIT_DEFAULTS
        },
	{ { ONLP_THERMAL_ID_CREATE(LEFT_FRONT_THERMAL_5), "Left Front Thermal Sensor 5", 0},
            ONLP_THERMAL_STATUS_PRESENT,
            ONLP_THERMAL_CAPS_ALL, 0, SDX8305_THERMAL_THRESHOLD_INIT_DEFAULTS
        },
	{ { ONLP_THERMAL_ID_CREATE(DENVERTON_THERMAL_6), "Denverton Thermal Sensor 6", 0},
            ONLP_THERMAL_STATUS_PRESENT,
            ONLP_THERMAL_CAPS_ALL, 0, SDX8305_THERMAL_THRESHOLD_INIT_DEFAULTS
        },
};


/*
 * This will be called to intiialize the thermali subsystem.
 */
int
onlp_thermali_init(void)
{
    return ONLP_STATUS_OK;
}

/*
 * Retrieve the information structure for the given thermal OID.
 *
 * If the OID is invalid, return ONLP_E_STATUS_INVALID.
 * If an unexpected error occurs, return ONLP_E_STATUS_INTERNAL.
 * Otherwise, return ONLP_STATUS_OK with the OID's information.
 *
 * Note -- it is expected that you fill out the information
 * structure even if the sensor described by the OID is not present.
 */
int
onlp_thermali_info_get(onlp_oid_t id, onlp_thermal_info_t* info)
{
    int local_id;
    VALIDATE(id);

    local_id = ONLP_OID_ID_GET(id);

    /* Set the onlp_oid_hdr_t and capabilities */
    *info = linfo[local_id];


    return onlp_file_read_int(&info->mcelsius, tempfiles__[local_id]);
}
