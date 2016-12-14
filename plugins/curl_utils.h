#pragma once

#include <glib.h>
#include <jansson.h>

#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#endif

json_t *json_registry_source_request(const gchar *url);
gchar *get_source_from_registry_by_id(const gchar *registry_url, const gchar *id);


