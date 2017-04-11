#define TRACE_MODULE _pgw_init

#include "core_debug.h"
#include "core_thread.h"

#include "pgw_context.h"
#include "pgw_event.h"

static thread_id pgw_sm_thread;
void *THREAD_FUNC pgw_sm_main(thread_id id, void *data);

status_t pgw_initialize()
{
    status_t rv;

    rv = pgw_context_init();
    if (rv != CORE_OK) return rv;

    rv = thread_create(&pgw_sm_thread, NULL, pgw_sm_main, NULL);
    if (rv != CORE_OK) return rv;

    return CORE_OK;
}

void pgw_terminate(void)
{
    thread_delete(pgw_sm_thread);

    pgw_context_final();
}

void *THREAD_FUNC pgw_sm_main(thread_id id, void *data)
{
    event_t event;
    fsm_t pgw_sm;
    c_time_t prev_tm, now_tm;
    int r;

    memset(&event, 0, sizeof(event_t));

    pgw_self()->queue_id = event_create();
    d_assert(pgw_self()->queue_id, return NULL, 
            "PGW event queue creation failed");
    tm_service_init(&pgw_self()->tm_service);
    gtp_xact_init(&pgw_self()->gtp_xact_ctx, 
            &pgw_self()->tm_service, EVT_TM_PGW_T3);

    fsm_create(&pgw_sm, pgw_state_initial, pgw_state_final);
    fsm_init(&pgw_sm, 0);

    prev_tm = time_now();

    while ((!thread_should_stop()))
    {
        r = event_timedrecv(pgw_self()->queue_id, &event, EVENT_WAIT_TIMEOUT);

        d_assert(r != CORE_ERROR, continue,
                "While receiving a event message, error occurs");

        now_tm = time_now();

        /* if the gap is over 10 ms, execute preriodic jobs */
        if (now_tm - prev_tm > EVENT_WAIT_TIMEOUT)
        {
            tm_execute_tm_service(
                    &pgw_self()->tm_service, pgw_self()->queue_id);

            prev_tm = now_tm;
        }

        if (r == CORE_TIMEUP)
        {
            continue;
        }

        fsm_dispatch(&pgw_sm, (fsm_event_t*)&event);
    }

    fsm_final(&pgw_sm, 0);
    fsm_clear(&pgw_sm);

    gtp_xact_final();
    event_delete(pgw_self()->queue_id);

    return NULL;
}
