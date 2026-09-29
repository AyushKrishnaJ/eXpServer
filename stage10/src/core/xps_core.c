#include "xps_core.h"

xps_core_t *xps_core_create() {

    xps_core_t *core = malloc(sizeof(xps_core_t));/* allocate memory using malloc() */
    /* handle error where core == NULL */
    if (core == NULL) {
        logger(LOG_ERROR, "xps_core_create()", "failed to allocate memory for core");
        return NULL;
    }

    xps_loop_t *loop = xps_loop_create(core);/* create xps_loop instance */
    /* handle error where loop == NULL */
    if (loop == NULL) {
        logger(LOG_ERROR, "xps_core_create()", "failed to create a new loop instance");
        xps_core_destroy(core);
        return NULL;
    }

    // Init values
    core->loop = loop;/* fill this */
    vec_init(&(core->listeners));
    /* initialize core->connections */
    vec_init(&(core->connections));
    core->n_null_listeners = 0;
    /* initialize core->n_null_connections */
    core->n_null_listeners = 0;

    logger(LOG_DEBUG, "xps_core_create()", "created core");

    return core;
}

void xps_core_destroy(xps_core_t *core) {
    assert(core != NULL);

    // Destroy connections
    for (int i = 0; i < core->connections.length; i++) {
        xps_connection_t *connection = core->connections.data[i];
        if (connection != NULL)
            xps_connection_destroy(connection); // modification of xps_connection_destroy() will be look at later
    }
    vec_deinit(&(core->connections));

    /* destory all the listeners and de-initialize core->listeners */
    for (int i = 0; i < core->listeners.length; i++) {
        xps_listener_t *listener = core->listeners.data[i];
        if (listener != NULL)
            xps_listener_destroy(listener); // modification of xps_connection_destroy() will be look at later
    }
    vec_deinit(&(core->listeners));

    /* destory loop attached to core */
    xps_loop_destroy(core->loop);

    /* free core instance */
    free(core);

    logger(LOG_DEBUG, "xps_core_destroy()", "destroyed core");
}

void xps_core_start(xps_core_t *core) {
    /* validate params */
    assert(core != NULL);

    logger(LOG_DEBUG, "xps_start()", "starting core");

    /* create listeners from port 8001 to 8004 */
    for (int i = 8001; i <= 8004; i++) {
        xps_listener_create(core, "0.0.0.0", i);
        logger(LOG_INFO, "xps_core_start()", "listening on port %u", i);
    }

    /* run loop instance using xps_loop_run() */
    xps_loop_run(core->loop);
}