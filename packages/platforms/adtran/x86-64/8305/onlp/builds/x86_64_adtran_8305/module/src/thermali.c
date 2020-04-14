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

#define THERMAL_WARNING 0
#define THERMAL_ERROR 1
#define THERMAL_SHUTDOWN 2
    



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

static char* threshold_files__[][40] =
{
    {
        "reserved",
        "/sys/class/hwmon/hwmon2/temp1_max",
        "/sys/class/hwmon/hwmon2/temp2_max",
        "/sys/class/hwmon/hwmon2/temp3_max",
        "/sys/class/hwmon/hwmon2/temp4_max",
        "/sys/class/hwmon/hwmon2/temp5_max",
        "/sys/class/hwmon/hwmon2/temp6_max"
    },
    {
        "reserved",
        "/sys/class/hwmon/hwmon2/temp1_critical",
        "/sys/class/hwmon/hwmon2/temp2_critical",
        "/sys/class/hwmon/hwmon2/temp3_critical",
        "/sys/class/hwmon/hwmon2/temp4_critical",
        "/sys/class/hwmon/hwmon2/temp5_critical",
        "/sys/class/hwmon/hwmon2/temp6_critical"
    },
    {
        "reserved",
        "/sys/class/hwmon/hwmon2/temp1_emergency",
        "/sys/class/hwmon/hwmon2/temp2_emergency",
        "/sys/class/hwmon/hwmon2/temp3_emergency",
        "/sys/class/hwmon/hwmon2/temp4_emergency",
        "/sys/class/hwmon/hwmon2/temp5_emergency",
        "/sys/class/hwmon/hwmon2/temp6_emergency"
    },
};


/* Static values */
static onlp_thermal_info_t linfo[] = {
	{ }, /* Not used */
	{ 
        { ONLP_THERMAL_ID_CREATE(THERMAL_CPU_CORE), "CPU Core", 0},
            ONLP_THERMAL_STATUS_PRESENT,
            ONLP_THERMAL_CAPS_ALL, 0, 
            get_threshold(THERMAL_WARNING, 0),       
            get_threshold(THERMAL_ERROR, 0),          
            get_threshold(THERMAL_SHUTDOWN, 0) 
    },
	{ 
        { ONLP_THERMAL_ID_CREATE(THERMAL_1_ON_MAIN_BROAD), "Chassis Thermal Sensor 1", 0},
            ONLP_THERMAL_STATUS_PRESENT,
            ONLP_THERMAL_CAPS_ALL, 0,             
            get_threshold(THERMAL_WARNING, 1),       
            get_threshold(THERMAL_ERROR, 1),          
            get_threshold(THERMAL_SHUTDOWN, 1) 
        },
	{ { ONLP_THERMAL_ID_CREATE(THERMAL_2_ON_MAIN_BROAD), "Chassis Thermal Sensor 2", 0},
            ONLP_THERMAL_STATUS_PRESENT,
            ONLP_THERMAL_CAPS_ALL, 0,             
            get_threshold(THERMAL_WARNING, 2),       
            get_threshold(THERMAL_ERROR, 2),          
            get_threshold(THERMAL_SHUTDOWN, 2) 
        },
	{ { ONLP_THERMAL_ID_CREATE(THERMAL_3_ON_MAIN_BROAD), "Chassis Thermal Sensor 3", 0},
            ONLP_THERMAL_STATUS_PRESENT,
            ONLP_THERMAL_CAPS_ALL, 0,             
            get_threshold(THERMAL_WARNING, 3),       
            get_threshold(THERMAL_ERROR, 3),          
            get_threshold(THERMAL_SHUTDOWN, 3) 
        },
	{ { ONLP_THERMAL_ID_CREATE(THERMAL_3_ON_MAIN_BROAD), "Chassis Thermal Sensor 4", 0},
            ONLP_THERMAL_STATUS_PRESENT,
            ONLP_THERMAL_CAPS_ALL, 0,             
            get_threshold(THERMAL_WARNING, 4),       
            get_threshold(THERMAL_ERROR, 4),          
            get_threshold(THERMAL_SHUTDOWN, 4) 
        },
	{ { ONLP_THERMAL_ID_CREATE(THERMAL_1_ON_PSU1), "PSU-1 Thermal Sensor 1", ONLP_PSU_ID_CREATE(PSU1_ID)},
            ONLP_THERMAL_STATUS_PRESENT,
            ONLP_THERMAL_CAPS_ALL, 0,             
            get_threshold(THERMAL_WARNING, 5),       
            get_threshold(THERMAL_ERROR, 5),          
            get_threshold(THERMAL_SHUTDOWN, 5) 
        },
};

int
get_threshold(int type, int oid)
{
    int value;

    value = onlp_file_read_int(*value, threshold_files__[type][oid]);
    return value;
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


    return onlp_file_read_int(&info->mcelsius, tempfiles__[local_id]);
}
