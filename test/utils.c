/*
 * Copyright (c) 2015      Intel, Inc.  All rights reserved.
 * Copyright (c) 2015-2017 Mellanox Technologies, Inc.
 *                         All rights reserved.
 * Copyright (c) 2016      Research Organization for Information Science
 *                         and Technology (RIST). All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 *
 */

#include "utils.h"
#include "test_common.h"
#include "pmix_server.h"
#include "cli_stages.h"

#define INFO_KV_SET(kv, _key, type, val) {               \
    (void)strncpy((kv)->key, _key, PMIX_MAX_KEYLEN);       \
    PMIX_VAL_SET(&(kv)->value, type, val);              \
}

static void release_cb(pmix_status_t status, void *cbdata)
{
    int *ptr = (int*)cbdata;
    *ptr = 0;
}

static void fill_seq_ranks_array(size_t nprocs, int base_rank, char **ranks)
{
    uint32_t i;
    int len = 0, max_ranks_len;
    if (0 >= nprocs) {
        return;
    }
    max_ranks_len = nprocs * (MAX_DIGIT_LEN+1);
    *ranks = (char*) malloc(max_ranks_len);
    for (i = 0; i < nprocs; i++) {
        len += snprintf(*ranks + len, max_ranks_len-len-1, "%d", i+base_rank);
        if (i != nprocs-1) {
            len += snprintf(*ranks + len, max_ranks_len-len-1, "%c", ',');
        }
    }
    if (len >= max_ranks_len-1) {
        free(*ranks);
        *ranks = NULL;
        TEST_ERROR(("Not enough allocated space for global ranks array."));
    }
}

static void set_namespace(int nprocs, char *ranks, char *name, int nproc_tot)
{
    size_t ninfo;
    pmix_info_t *info;
    int n;

    ninfo = 8 + nproc_tot;
    char *regex, *ppn;

    PMIX_INFO_CREATE(info, ninfo);

    INFO_KV_SET(&info[0], PMIX_UNIV_SIZE, uint32_t, /*nproc_tot*/ nprocs);
    INFO_KV_SET(&info[1], PMIX_SPAWNED, uint32_t, 0);
    INFO_KV_SET(&info[2], PMIX_LOCAL_SIZE, uint32_t, nprocs);
    INFO_KV_SET(&info[3], PMIX_LOCAL_PEERS, string, ranks);

    PMIx_generate_regex(NODE_NAME, &regex);
    INFO_KV_SET(&info[4], PMIX_NODE_MAP, string, regex);
    free(regex);

    PMIx_generate_ppn(ranks, &ppn);
    INFO_KV_SET(&info[5], PMIX_PROC_MAP, string, ppn);
    free(ppn);

    INFO_KV_SET(&info[6], PMIX_JOB_SIZE, uint32_t, /*nproc_tot*/ nprocs);
    INFO_KV_SET(&info[7], PMIX_APPNUM, uint32_t, 0);

    /* emulate more procs tnan we have
     * to stress dstore
     */
    for (n=0; n < nproc_tot; n++) {
        pmix_info_t *kvp = &(info[8 + n]);
        pmix_info_t *info1;
        char hname[256];
        int count = 5, k = 0;
        PMIX_INFO_CREATE(info1, count);

        strcpy(kvp->key, PMIX_PROC_DATA);
        kvp->value.type = PMIX_DATA_ARRAY;
        kvp->value.data.darray = (pmix_data_array_t*)calloc(1, sizeof(pmix_data_array_t));
        kvp->value.data.darray->type = PMIX_INFO;
        kvp->value.data.darray->array = (void*)info1;
        kvp->value.data.darray->size = count;

        /* must start with rank */
        INFO_KV_SET(&info1[k], PMIX_RANK, int, n);
        k++;
        INFO_KV_SET(&info1[k], PMIX_LOCAL_RANK, uint16_t, n % nprocs);
        k++;
        INFO_KV_SET(&info1[k], PMIX_NODE_RANK, uint32_t, n / nprocs);
        k++;
        INFO_KV_SET(&info1[k], PMIX_NODEID, uint32_t, n / nprocs);
        k++;
        sprintf(hname,"cluster-node-%d",n / nprocs);
        INFO_KV_SET(&info1[k], PMIX_HOSTNAME, string, hname);
    }

    {
        int delay = 0;
        while(delay ){
            sleep(1);
        }
    }

    volatile int in_progress = 1, rc;
    if (PMIX_SUCCESS == (rc = PMIx_server_register_nspace(name, nprocs, info, ninfo, release_cb, (void*)&in_progress))) {
        PMIX_WAIT_FOR_COMPLETION(in_progress);
    }

    PMIX_INFO_FREE(info, ninfo);
}

void dereg_namespace(void)
{
    volatile int in_progress = 1;
    char nspace[PMIX_MAX_NSLEN];
    (void)snprintf(nspace, PMIX_MAX_NSLEN, "%s-%d", TEST_NAMESPACE, 0);

    {
        int delay = 0;
        while( delay ){
            sleep(1);
        }
    }

    PMIx_server_deregister_nspace(nspace, release_cb, (void*)&in_progress);
    PMIX_WAIT_FOR_COMPLETION(in_progress);
}

void set_client_argv(test_params *params, char ***argv)
{
    pmix_argv_append_nosize(argv, params->binary);
    pmix_argv_append_nosize(argv, "-n");
    if (NULL == params->np) {
        pmix_argv_append_nosize(argv, "1");
    } else {
        pmix_argv_append_nosize(argv, params->np);
    }
    if( params->verbose ){
        pmix_argv_append_nosize(argv, "-v");
    }
    if (NULL != params->prefix) {
        pmix_argv_append_nosize(argv, "-o");
        pmix_argv_append_nosize(argv, params->prefix);
    }
    if( params->early_fail ){
        pmix_argv_append_nosize(argv, "--early-fail");
    }
    if (NULL != params->fences) {
        pmix_argv_append_nosize(argv, "--fence");
        pmix_argv_append_nosize(argv, params->fences);
        if (params->use_same_keys) {
            pmix_argv_append_nosize(argv, "--use-same-keys");
        }
    }
    if (params->test_job_fence) {
        pmix_argv_append_nosize(argv, "--job-fence");
        if (params->nonblocking) {
            pmix_argv_append_nosize(argv, "-nb");
        }
        if (params->collect) {
            pmix_argv_append_nosize(argv, "-c");
        }
        if (params->collect_bad) {
            pmix_argv_append_nosize(argv, "--collect-corrupt");
        }
    }
    if (NULL != params->noise) {
        pmix_argv_append_nosize(argv, "--noise");
        pmix_argv_append_nosize(argv, params->noise);
    }
    if (NULL != params->ns_dist) {
        pmix_argv_append_nosize(argv, "--ns-dist");
        pmix_argv_append_nosize(argv, params->ns_dist);
    }
    if (params->test_publish) {
        pmix_argv_append_nosize(argv, "--test-publish");
    }
    if (params->test_spawn) {
        pmix_argv_append_nosize(argv, "--test-spawn");
    }
    if (params->test_connect) {
        pmix_argv_append_nosize(argv, "--test-connect");
    }
    if (params->test_resolve_peers) {
        pmix_argv_append_nosize(argv, "--test-resolve-peers");
    }
    if (params->test_error) {
        pmix_argv_append_nosize(argv, "--test-error");
    }
    if (params->key_replace) {
        pmix_argv_append_nosize(argv, "--test-replace");
        pmix_argv_append_nosize(argv, params->key_replace);
    }
    if (params->test_internal) {
        char tmp[32];
        sprintf(tmp, "%d", params->test_internal);
        pmix_argv_append_nosize(argv, "--test-internal");
        pmix_argv_append_nosize(argv, tmp);
    }
}

int launch_clients(int num_procs, char *binary, char *** client_env, char ***base_argv)
{
    int n;
    uid_t myuid;
    gid_t mygid;
    char *ranks = NULL;
    char digit[MAX_DIGIT_LEN];
    int rc;
    static int counter = 0;
    static int num_ns = 0;
    pmix_proc_t proc;

    TEST_VERBOSE(("Setting job info"));
    fill_seq_ranks_array(num_procs, counter, &ranks);
    if (NULL == ranks) {
        PMIx_server_finalize();
        TEST_ERROR(("fill_seq_ranks_array failed"));
        return PMIX_ERROR;
    }
    (void)snprintf(proc.nspace, PMIX_MAX_NSLEN, "%s-%d", TEST_NAMESPACE, num_ns);
    set_namespace(num_procs, ranks, proc.nspace, 4096 * 64);
    if (NULL != ranks) {
        free(ranks);
    }

    myuid = getuid();
    mygid = getgid();

    /* fork/exec the test */
    for (n = 0; n < num_procs; n++) {
        proc.rank = counter;
        if (PMIX_SUCCESS != (rc = PMIx_server_setup_fork(&proc, client_env))) {//n
            TEST_ERROR(("Server fork setup failed with error %d", rc));
            PMIx_server_finalize();
            cli_kill_all();
            return rc;
        }
        if (PMIX_SUCCESS != (rc = PMIx_server_register_client(&proc, myuid, mygid, NULL, NULL, NULL))) {//n
            TEST_ERROR(("Server fork setup failed with error %d", rc));
            PMIx_server_finalize();
            cli_kill_all();
            return rc;
        }

        cli_info[counter].pid = fork();
        if (cli_info[counter].pid < 0) {
            TEST_ERROR(("Fork failed"));
            PMIx_server_finalize();
            cli_kill_all();
            return -1;
        }
        cli_info[counter].rank = counter;//n
        cli_info[counter].ns = strdup(proc.nspace);

        char **client_argv = pmix_argv_copy(*base_argv);

        /* add two last arguments: -r <rank> */
        sprintf(digit, "%d", counter);//n
        pmix_argv_append_nosize(&client_argv, "-r");
        pmix_argv_append_nosize(&client_argv, digit);

        pmix_argv_append_nosize(&client_argv, "-s");
        pmix_argv_append_nosize(&client_argv, proc.nspace);

        sprintf(digit, "%d", num_procs);
        pmix_argv_append_nosize(&client_argv, "--ns-size");
        pmix_argv_append_nosize(&client_argv, digit);

        sprintf(digit, "%d", num_ns);
        pmix_argv_append_nosize(&client_argv, "--ns-id");
        pmix_argv_append_nosize(&client_argv, digit);

        sprintf(digit, "%d", (counter-n));
        pmix_argv_append_nosize(&client_argv, "--base-rank");
        pmix_argv_append_nosize(&client_argv, digit);

        if (cli_info[counter].pid == 0) {
            if( !TEST_VERBOSE_GET() ){
                // Hide clients stdout
                freopen("/dev/null","w", stdout);
            }
            execve(binary, client_argv, *client_env);
            /* Does not return */
            exit(0);
        }
        cli_info[counter].state = CLI_FORKED;

        pmix_argv_free(client_argv);

        counter++;
    }
    num_ns++;
    return PMIX_SUCCESS;
}
