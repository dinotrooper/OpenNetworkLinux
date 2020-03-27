/************************************************************
 * <bsn.cl fy=2014 v=onl>
 *
 *        Copyright 2014, 2015 Big Switch Networks, Inc.
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
 *
 *
 ***********************************************************/
#include <onlp/sys.h>
#include <onlp/platformi/sysi.h>
#include <onlplib/mmap.h>
#include <AIM/aim.h>
#include "onlp_log.h"
#include "onlp_int.h"
#include "onlp_locks.h"

static char*
platform_detect_fs__(int warn)
{
    /*
     * Check the filesystem for the platform identifier.
     */
    char* rv = NULL;
    if(ONLP_CONFIG_PLATFORM_FILENAME) {
        FILE* fp;
        if((fp=fopen(ONLP_CONFIG_PLATFORM_FILENAME, "r"))) {
            char platform[256];
            if(fgets(platform, sizeof(platform), fp) == platform) {
                /* TODO: Base this detection on the global platform registry. */
                if(platform[0]) {
                    if(platform[ONLP_STRLEN(platform)-1] == '\n') {
                        platform[ONLP_STRLEN(platform)-1] = 0;
                    }
                    rv = aim_strdup(platform);
                }
            }
            fclose(fp);
        }
        else {
            if(warn) {
                AIM_LOG_WARN("could not open platform filename '%s'", ONLP_CONFIG_PLATFORM_FILENAME);
            }
        }
    }
    return rv;
}

static char*
platform_detect__(void)
{
#if ONLP_CONFIG_INCLUDE_PLATFORM_STATIC == 1
    return aim_strdup(ONLP_CONFIG_PLATFORM_STATIC);
#endif
    return platform_detect_fs__(1);
}

static int
onlp_sys_init_locked__(void)
{
    int rv;
    printf("\n(INTEGRAL) - onlp_sys_init(debug 1)\n");
    const char* current_platform = platform_detect__();
    printf("(INTEGRAL) - onlp_sys_init(debug 2) -- current_platform = %s\n", current_platform);
    if(current_platform == NULL) {
            printf("(INTEGRAL) - onlp_sys_init(debug 2.5)\n");
        AIM_DIE("Could not determine the current platform.");
    }
    const char* current_interface = onlp_sysi_platform_get();
    printf("(INTEGRAL) - onlp_sys_init(debug 3)\n");
    if(current_interface == NULL) {
        printf("(INTEGRAL) - onlp_sys_init(debug 3.5)\n");
        AIM_DIE("The platform driver did not return an appropriate platform identifier.");
    }
    printf("WARNING (integral): Current_platform = %s,  Current_interface = %s\n", current_platform, current_interface);
    printf("(INTEGRAL) - onlp_sys_init(debug 4)\n");

    if(strcmp(current_interface, current_platform)) {
        /* They do not match. Ask the interface if it supports the current platform. */
        printf("(INTEGRAL) - onlp_sys_init(debug 5)\n");
        int rv = onlp_sysi_platform_set(current_platform);
        printf("(INTEGRAL) - onlp_sys_init(debug 6)\n");
        if(rv < 0) {
            printf("(INTEGRAL) - onlp_sys_init(debug 7)\n");
            AIM_DIE("The current platform interface (%s) does not support the current platform (%s). This is fatal.",
                    current_interface, current_platform);
        }
    }

    /* If we get here, its all good */
    printf("(INTEGRAL) - onlp_sys_init(debug 8) 'if we get here its all good (:'\n");
    aim_free((char*)current_platform);
    printf("(INTEGRAL) - onlp_sys_info(debug 9)\n");
    rv = onlp_sysi_init();
    printf("(INTEGRAL) - onlp_sys_info(debug 10)\n");
    return rv;
}
ONLP_LOCKED_API0(onlp_sys_init);

static uint8_t*
onie_data_get__(int* free)
{
    void* pa;
    uint8_t* ma = NULL;
    int size;
    if(onlp_sysi_onie_data_phys_addr_get(&pa) == 0) {
        ma = onlp_mmap((off_t)pa, 64*1024, "onie_data_get__");
        *free = 0;
    }
    else if(onlp_sysi_onie_data_get(&ma, &size) == 0) {
        *free = 1;
    }
    else {
        ma = NULL;
        *free = 0;
    }
    return ma;
}

static int
onlp_sys_info_get_locked__(onlp_sys_info_t* rv)
{
    if(rv == NULL) {
        return -1;
    }

    memset(rv, 0, sizeof(*rv));

    /**
     * Get the system ONIE information.
     */
    int free;
    uint8_t* onie_data = onie_data_get__(&free);

    if(onie_data) {
        onlp_onie_decode(&rv->onie_info, onie_data, -1);
        if(free) {
            onlp_sysi_onie_data_free(onie_data);
        }
    }
    else {
        if(onlp_sysi_onie_info_get(&rv->onie_info) != 0) {
            memset(&rv->onie_info, 0, sizeof(rv->onie_info));
            list_init(&rv->onie_info.vx_list);
        }
    }

    /*
     * Query the sys oids
     */
    onlp_sysi_oids_get(rv->hdr.coids, AIM_ARRAYSIZE(rv->hdr.coids));

    /*
     * Platform Information
     */
    onlp_sysi_platform_info_get(&rv->platform_info);

    return 0;
}
ONLP_LOCKED_API1(onlp_sys_info_get,onlp_sys_info_t*,rv);

void
onlp_sys_info_free(onlp_sys_info_t* info)
{
    onlp_onie_info_free(&info->onie_info);
    onlp_sysi_platform_info_free(&info->platform_info);
}

static int
onlp_sys_hdr_get_locked__(onlp_oid_hdr_t* hdr)
{
    memset(hdr, 0, sizeof(*hdr));
    return onlp_sysi_oids_get(hdr->coids, AIM_ARRAYSIZE(hdr->coids));
}
ONLP_LOCKED_API1(onlp_sys_hdr_get, onlp_oid_hdr_t*, hdr);


void
onlp_sys_dump(onlp_oid_t id, aim_pvs_t* pvs, uint32_t flags)
{
    int rv;
    iof_t iof;
    onlp_sys_info_t si;

    onlp_oid_dump_iof_init_default(&iof, pvs);

    if(ONLP_OID_TYPE_GET(id) != ONLP_OID_TYPE_SYS) {
        return;
    }

    iof_push(&iof, "System Information:");
    rv = onlp_sys_info_get(&si);
    if(rv < 0) {
        onlp_oid_info_get_error(&iof, rv);
        iof_pop(&iof);
        return;
    }
    else {
        onlp_onie_show(&si.onie_info, &iof.inherit);
        iof_pop(&iof);
    }
    onlp_oid_table_dump(si.hdr.coids, pvs, flags);
    onlp_sys_info_free(&si);
}

void
onlp_sys_show(onlp_oid_t id, aim_pvs_t* pvs, uint32_t flags)
{
    int rv;
    iof_t iof;
    onlp_sys_info_t si;
    int yaml;

    onlp_oid_show_iof_init_default(&iof, pvs, flags);
    yaml = (flags & ONLP_OID_SHOW_YAML);

    if(id && ONLP_OID_TYPE_GET(id) != ONLP_OID_TYPE_SYS) {
        return;
    }

    rv = onlp_sys_info_get(&si);
    if(rv < 0) {
        onlp_oid_info_get_error(&iof, rv);
        return;
    }

#define YPUSH(key) do { if(yaml) { iof_push(&iof, key); } } while(0)
#define YPOP()     do { if(yaml) { iof_pop(&iof); } } while(0)

    /*
     * The system information is not actually shown
     * unless you specify EXTENDED or !RECURSIVE
     */
    if(yaml ||
       flags & ONLP_OID_SHOW_EXTENDED ||
       (flags & ONLP_OID_SHOW_RECURSE) == 0) {
        iof_push(&iof, "System Information:");
        onlp_onie_show(&si.onie_info, &iof.inherit);
        iof_pop(&iof);
    }

    if(flags & ONLP_OID_SHOW_RECURSE) {

        onlp_oid_t* oidp;

        /** Show all Chassis Fans */
        YPUSH("Fans:");
        ONLP_OID_TABLE_ITER_TYPE(si.hdr.coids, oidp, FAN) {
            onlp_oid_show(*oidp, &iof.inherit, flags);
        }
        YPOP();

        /** Show all System Thermals */
        YPUSH("Thermals:");
        ONLP_OID_TABLE_ITER_TYPE(si.hdr.coids, oidp, THERMAL) {
            onlp_oid_show(*oidp, &iof.inherit, flags);
        }
        YPOP();

        /** Show all PSUs */
        YPUSH("PSUs:");
        ONLP_OID_TABLE_ITER_TYPE(si.hdr.coids, oidp, PSU) {
            onlp_oid_show(*oidp, &iof.inherit, flags);
        }
        YPOP();

        if(flags & ONLP_OID_SHOW_EXTENDED) {
            /** Show all LEDs */
            YPUSH("LEDs:");
            ONLP_OID_TABLE_ITER_TYPE(si.hdr.coids, oidp, LED) {
                onlp_oid_show(*oidp, &iof.inherit, flags);
            }
            YPOP();
        }
    }
    onlp_sys_info_free(&si);
}

int
onlp_sys_ioctl(int code, ...)
{
    int rv;
    va_list vargs;
    va_start(vargs, code);
    rv = onlp_sys_vioctl(code, vargs);
    va_end(vargs);
    return rv;
}

static int
onlp_sys_vioctl_locked__(int code, va_list vargs)
{
    return onlp_sysi_ioctl(code, vargs);
}
ONLP_LOCKED_API2(onlp_sys_vioctl, int, code, va_list, vargs);

static int
onlp_sys_debug_locked__(aim_pvs_t* pvs, int argc, char* argv[])
{
    return onlp_sysi_debug(pvs, argc, argv);
}
ONLP_LOCKED_API3(onlp_sys_debug, aim_pvs_t*, pvs, int, argc, char**, argv);
