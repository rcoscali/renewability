#include <accl.h>
#include "renewability.h"

struct libwebsocket_context* rn_ws_channel = NULL;
pthread_t monitor_thread;

/*
 * This callback is responsible for incoming renewability requests handling
 *
 * Upon a request the client may have to update certain blocks within a
 * given timeout
 */
void* rn_callback(void* in_buffer, size_t len) {
    rn_message* request_from_manager;
    rn_renewblock* renew_request;

#ifndef NDEBUG
    lwsl_notice("RENEWABILITY callback invoked. %d %d", sizeof(rn_message), len);
#endif

    /* The payload has to respect renewability messages format */
    if (len == sizeof(rn_message)) {
        request_from_manager = (rn_message*)in_buffer;

#ifndef NDEBUG
        lwsl_notice("RENEWABILITY callback invoked. type=%d\n", request_from_manager->type);
#endif

        switch (request_from_manager->type) {
            case RN_RENEW_BLOCK:

                /* a request to unbind a specific block has been received */
                renew_request = (rn_renewblock*)request_from_manager->buffer;

#ifndef NDEBUG
                lwsl_notice("RENEWABILITY RN_RENEW_BLOCK for block %d received\n", renew_request->block_index);
#endif

                /* call unbinder on requested (block_index) code block */
                EraseMobileBlock (renew_request->block_index, renew_request->code);

#ifndef NDEBUG
                lwsl_notice("RENEWABILITY block %d erased\n", renew_request->block_index);
#endif
                break;
            case RN_RENEW_ALLBLOCKS:
                lwsl_notice("RENEWABILITY RN_RENEW_ALLBLOCKS received\n");
                /* a request to unbind all has been received
                 * let's call universal unbinder
                 */
                EraseAllMobileBlocks();
#ifndef NDEBUG
                lwsl_notice("RENEWABILITY all blocks erased\n");
#endif
                break;
            default:
                /* unexpected request, do nothing */
                break;
        }
    } else {
#ifndef NDEBUG
        lwsl_err("RENEWABILITY unexpected message format\n");
#endif
    }

    return NULL;
}

/**
 * Renewability separate thread monitor
 */
void* renewability_monitor(void* buffer) {
    rn_ws_channel = acclWebSocketInit (ACCL_RENEWABILITY, rn_callback);

    if (NULL == rn_ws_channel) {
#ifndef NDEBUG
        lwsl_err("RENEWABILITY INITIALIZATION FAILED\n");
#endif
        /* channel initialization failed */
        return false;
    }

#ifndef NDEBUG
    lwsl_notice("RENEWABILITY INITIALIZATION COMPLETED\n");
#endif

    while (1) {
        libwebsocket_service(rn_ws_channel, RENEWABILITY_POLLING_INTERVAL);
    }
}

/* Enables Renewability client support */
pthread_mutex_t init_mutex;
static int initialized = 0;

bool renewabilityInit() {
    int t_res;

    pthread_mutex_lock(&init_mutex);

    /* the channel is already open */
    if (NULL != rn_ws_channel)
    if (initialized) {
        pthread_mutex_unlock(&init_mutex);
        return true;
    }
    
    initialized = 1;

    /* starts reneability thread */
    t_res = pthread_create(&monitor_thread, NULL, &renewability_monitor, NULL);

    if (0 == t_res) {
        /* renewability thread is running */

       pthread_mutex_unlock(&init_mutex);
        return true;
    }
 
#ifndef NDEBUG
    lwsl_notice("RENEWABILITY THREAD SPAWNED");
#endif
 
    pthread_mutex_unlock(&init_mutex);
    return false;
}

/* Disables Renewability client support */
bool renewabilityShutdown() {
    acclWebSocketShutdown(rn_ws_channel);

    rn_ws_channel = NULL;

    return true;
}
