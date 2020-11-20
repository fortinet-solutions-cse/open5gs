/*
 * Copyright (C) 2019 by Sukchan Lee <acetcom@gmail.com>
 *
 * This file is part of Open5GS.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "ogs-app.h"
#include "ogs-sbi.h"

extern const ogs_sbi_server_actions_t ogs_mhd_server_actions;
extern const ogs_sbi_server_actions_t ogs_nghttp2_server_actions;

ogs_sbi_server_actions_t ogs_sbi_server_actions;
bool ogs_sbi_server_actions_initialized = false;

static OGS_POOL(server_pool, ogs_sbi_server_t);

void ogs_sbi_server_init(int num_of_session_pool)
{
    if (ogs_sbi_server_actions_initialized == false) {
        ogs_sbi_server_actions = ogs_nghttp2_server_actions;
    }
        ogs_sbi_server_actions = ogs_mhd_server_actions;

    ogs_sbi_server_actions.init(num_of_session_pool);

    ogs_list_init(&ogs_sbi_self()->server_list);
    ogs_pool_init(&server_pool, ogs_app()->pool.nf);
}

void ogs_sbi_server_final(void)
{
    ogs_sbi_server_remove_all();

    ogs_pool_final(&server_pool);

    ogs_sbi_server_actions.cleanup();
}

ogs_sbi_server_t *ogs_sbi_server_add(ogs_sockaddr_t *addr)
{
    ogs_sbi_server_t *server = NULL;

    ogs_assert(addr);

    ogs_pool_alloc(&server_pool, &server);
    ogs_assert(server);
    memset(server, 0, sizeof(ogs_sbi_server_t));

    ogs_copyaddrinfo(&server->node.addr, addr);

    ogs_list_add(&ogs_sbi_self()->server_list, server);

    return server;
}

void ogs_sbi_server_remove(ogs_sbi_server_t *server)
{
    ogs_assert(server);

    ogs_list_remove(&ogs_sbi_self()->server_list, server);

    ogs_assert(server->node.addr);
    ogs_freeaddrinfo(server->node.addr);

    ogs_pool_free(&server_pool, server);
}

void ogs_sbi_server_remove_all(void)
{
    ogs_sbi_server_t *server = NULL, *next_server = NULL;

    ogs_list_for_each_safe(&ogs_sbi_self()->server_list, next_server, server)
        ogs_sbi_server_remove(server);
}

void ogs_sbi_server_start_all(
        int (*cb)(ogs_sbi_request_t *request, void *data))
{
    ogs_sbi_server_t *server = NULL, *next_server = NULL;

    ogs_list_for_each_safe(&ogs_sbi_self()->server_list, next_server, server)
        ogs_sbi_server_actions.start(server, cb);
}

void ogs_sbi_server_stop_all(void)
{
    ogs_sbi_server_t *server = NULL, *next_server = NULL;

    ogs_list_for_each_safe(&ogs_sbi_self()->server_list, next_server, server)
        ogs_sbi_server_actions.stop(server);
}

void ogs_sbi_server_send_response(
        ogs_sbi_stream_t *session, ogs_sbi_response_t *response)
{
    ogs_sbi_server_actions.send_response(session, response);
}

void ogs_sbi_server_send_problem(
        ogs_sbi_stream_t *session, OpenAPI_problem_details_t *problem)
{
    ogs_sbi_message_t message;
    ogs_sbi_response_t *response = NULL;

    ogs_assert(session);
    ogs_assert(problem);

    memset(&message, 0, sizeof(message));

    message.http.content_type = (char*)"application/problem+json";
    message.ProblemDetails = problem;

    response = ogs_sbi_build_response(&message, problem->status);
    ogs_assert(response);

    ogs_sbi_server_send_response(session, response);
}

void ogs_sbi_server_send_error(ogs_sbi_stream_t *session,
        int status, ogs_sbi_message_t *message,
        const char *title, const char *detail)
{
    OpenAPI_problem_details_t problem;

    ogs_assert(session);

    memset(&problem, 0, sizeof(problem));

    if (message) {
        problem.type = ogs_msprintf("/%s/%s",
                message->h.service.name, message->h.api.version);
        if (message->h.resource.component[1])
            problem.instance = ogs_msprintf("/%s/%s",
                    message->h.resource.component[0],
                    message->h.resource.component[1]);
        else
            problem.instance =
                    ogs_msprintf("/%s", message->h.resource.component[0]);
    }
    problem.status = status;
    problem.title = (char*)title;
    problem.detail = (char*)detail;

    ogs_sbi_server_send_problem(session, &problem);

    if (problem.type)
        ogs_free(problem.type);
    if (problem.instance)
        ogs_free(problem.instance);
}

ogs_sbi_server_t *ogs_sbi_server_from_session(ogs_sbi_stream_t *session)
{
    return ogs_sbi_server_actions.from_session(session);
}

ogs_sbi_server_t *ogs_sbi_server_from_stream(ogs_sbi_stream_t *stream)
{
    return ogs_sbi_server_actions.from_session(stream);
}
