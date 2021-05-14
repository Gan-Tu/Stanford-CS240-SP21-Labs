#ifndef TEST_MOCK_H
#define TEST_MOCK_H

#include "common.h"
#include "client.h"

#define SERVE_DIR "/snfs/serve/"

client_state *MOCK_STATE;

bool start_server(bool);
bool stop_server(bool);

bool setup_client();
bool teardown_client();

bool start_mock_server();
bool stop_mock_server();
bool start_mock_client();
bool stop_mock_client();

pid_t server_pid;

#endif
