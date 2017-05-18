/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2015-2017 Intel, Inc. All rights reserved.
 * Copyright (c) 2016-2017 Mellanox Technologies, Inc.
 *                         All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include <src/include/pmix_config.h>

#include <pmix_common.h>
#include "src/include/pmix_globals.h"

#include "src/class/pmix_list.h"
#include "src/util/argv.h"
#include "src/util/error.h"

#include "src/mca/gds/base/base.h"


char* pmix_gds_base_get_available_modules(void)
{
    if (!pmix_gds_globals.initialized) {
        return NULL;
    }

    return strdup(pmix_gds_globals.all_mods);
}

/* Select a gds module per the given directives */
pmix_gds_base_module_t* pmix_gds_base_assign_module(pmix_info_t *info, size_t ninfo)
{
    pmix_gds_base_active_module_t *active;
    pmix_gds_base_module_t *mod = NULL;
    int pri, priority = -1;
    char *mod_name = NULL;

    if (!pmix_gds_globals.initialized) {
        return NULL;
    }

    if (NULL != info) {
        int n;
        for (n=0; n < ninfo; n++) {
            if (0 == strncmp(info[n].key, PMIX_GDS_MODULE, PMIX_MAX_KEYLEN)) {
                mod_name = info[n].value.data.string;
                break;
            }
        }
    }


    PMIX_LIST_FOREACH(active, &pmix_gds_globals.actives, pmix_gds_base_active_module_t) {
        if (NULL == active->module->assign_module) {
            continue;
        }

        /* If particular module name was specified - ignore all others */
        if( mod_name ){
            if( strcmp(mod_name, active->module->name) ){
                continue;
            }
        }

        if (PMIX_SUCCESS == active->module->assign_module(info, ninfo, &pri)) {
            if (pri < 0) {
                /* use the default priority from the component */
                pri = active->pri;
            }
            if (priority < pri) {
                mod = active->module;
                priority = pri;
            }
        }
    }

    return mod;
}

pmix_status_t pmix_gds_base_setup_fork(const pmix_proc_t *proc,
                                       char ***env)
{
    pmix_gds_base_active_module_t *active;
    pmix_status_t rc;

    if (!pmix_gds_globals.initialized) {
        return PMIX_ERR_INIT;
    }

    PMIX_LIST_FOREACH(active, &pmix_gds_globals.actives, pmix_gds_base_active_module_t) {
        if (NULL == active->module->setup_fork) {
            continue;
        }
        if (PMIX_SUCCESS != (rc = active->module->setup_fork(proc, env))) {
            return rc;
        }
    }

    return PMIX_SUCCESS;
}
