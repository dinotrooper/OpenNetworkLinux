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

#define THERMAL_WARNING = 0,
#define THERMAL_ERROR = 1,
#define THERMAL_SHUTDOWN =2,

#define GET_ONLP_THERMAL_THRESHHOLD(_oid)   \
    { get_threshold(THERMAL_WARNING, _oid),        \
      get_threshold(THERMAL_ERROR, _oid),          \
      get_threshold(THERMAL_SHUTDOWN, _oid) }    



enum onlp_thermal_id
{
    THERMAL_RESERVED = 0,
    THERMAL_CPU_CORE,
    THERMAL_1_ON_MAIN_BROAD,
    THERMAL_2_ON_MAIN_BROAD,
    THERMAL_3_ON_MAIN_BROAD,
    THERMAL_1_ON_PSU1,
    THERMAL_1_ON_PSU2,
};

enum onlp_thermal_value_type
{
    THERMAL_WARNING = 0,
    THERMAL_ERROR = 1,
    THERMAL_SHUTDOWN = 2,
};

static char* tempfiles__[] =  /* must map with onlp_thermal_id */
{
    "reserved",
    NULL,                  
    "/sys/cass/hwmon/hwmon2/temp1_label",
    "/sys/cass/hwmon/hwmon2/temp2_label",
    "/sys/cass/hwmon/hwmon2/temp3_label",
    "/sys/cass/hwmon/hwmon2/temp4_label",
    "/sys/cass/hwmon/hwmon2/temp5_label",
    "/sys/cass/hwmon/hwmon2/temp6_label",
};

static char* threshold_files[3][8]
{
    {
    "reserved",
    NULL,                 
    "/sys/cass/hwmon/hwmon2/temp1_max",
    "/sys/cass/hwmon/hwmon2/temp2_max",
    "/sys/cass/hwmon/hwmon2/temp3_max",
    "/sys/cass/hwmon/hwmon2/temp4_max",
    "/sys/cass/hwmon/hwmon2/temp5_max",
    "/sys/cass/hwmon/hwmon2/temp6_max"},
    {
    "reserved",
    NULL,                  
    "/sys/cass/hwmon/hwmon2/temp1_critical",
    "/sys/cass/hwmon/hwmon2/temp2_critical",
    "/sys/cass/hwmon/hwmon2/temp3_critical",
    "/sys/cass/hwmon/hwmon2/temp4_critical",
    "/sys/cass/hwmon/hwmon2/temp5_critical",
    "/sys/cass/hwmon/hwmon2/temp6_critical"},
    "reserved",
    {
    NULL,               
    "/sys/cass/hwmon/hwmon2/temp1_emergency",
    "/sys/cass/hwmon/hwmon2/temp2_emergency",
    "/sys/cass/hwmon/hwmon2/temp3_emergency",
    "/sys/cass/hwmon/hwmon2/temp4_emergency",
    "/sys/cass/hwmon/hwmon2/temp5_emergency",
    "/sys/cass/hwmon/hwmon2/temp6_emergency"},
}

/* Static values */
static onlp_thermal_info_t linfo[] = {
	{ }, /* Not used */
	{ { ONLP_THERMAL_ID_CREATE(THERMAL_CPU_CORE), "CPU Core", 0},
            ONLP_THERMAL_STATUS_PRESENT,
            ONLP_THERMAL_CAPS_ALL, 0, GET_ONLP_THERMAL_THRESHHOLD(1)
        },
	{ { ONLP_THERMAL_ID_CREATE(THERMAL_1_ON_MAIN_BROAD), "Chassis Thermal Sensor 1", 0},
            ONLP_THERMAL_STATUS_PRESENT,
            ONLP_THERMAL_CAPS_ALL, 0, GET_ONLP_THERMAL_THRESHHOLD(2)
        },
	{ { ONLP_THERMAL_ID_CREATE(THERMAL_2_ON_MAIN_BROAD), "Chassis Thermal Sensor 2", 0},
            ONLP_THERMAL_STATUS_PRESENT,
            ONLP_THERMAL_CAPS_ALL, 0, GET_ONLP_THERMAL_THRESHHOLD(3)
        },
	{ { ONLP_THERMAL_ID_CREATE(THERMAL_3_ON_MAIN_BROAD), "Chassis Thermal Sensor 3", 0},
            ONLP_THERMAL_STATUS_PRESENT,
            ONLP_THERMAL_CAPS_ALL, 0, GET_ONLP_THERMAL_THRESHHOLD(4)
        },
	{ { ONLP_THERMAL_ID_CREATE(THERMAL_3_ON_MAIN_BROAD), "Chassis Thermal Sensor 4", 0},
            ONLP_THERMAL_STATUS_PRESENT,
            ONLP_THERMAL_CAPS_ALL, 0, GET_ONLP_THERMAL_THRESHHOLD(5)
        },
	{ { ONLP_THERMAL_ID_CREATE(THERMAL_1_ON_PSU1), "PSU-1 Thermal Sensor 1", ONLP_PSU_ID_CREATE(PSU1_ID)},
            ONLP_THERMAL_STATUS_PRESENT,
            ONLP_THERMAL_CAPS_ALL, 0, GET_ONLP_THERMAL_THRESHHOLD(6)
        },
	{ { ONLP_THERMAL_ID_CREATE(THERMAL_1_ON_PSU2), "PSU-2 Thermal Sensor 1", ONLP_PSU_ID_CREATE(PSU2_ID)},
            ONLP_THERMAL_STATUS_PRESENT,
            ONLP_THERMAL_CAPS_ALL, 0, GET_ONLP_THERMAL_THRESHHOLD(7)
        }
};

int
get_threshold(int type, int oid)
{
    return threshold_files[type][oid];
}

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


    return onlp_file_read_int(&info->mcelsius, devfiles__[local_id]);
}
