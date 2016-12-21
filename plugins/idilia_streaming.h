#pragma once

#include <glib.h>

void janus_streaming_send_watch_request(gchar * id, gpointer handle);
void janus_streaming_send_destroy_request(gchar * id, gpointer handle);
