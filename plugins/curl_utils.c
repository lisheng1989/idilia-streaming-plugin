#include "curl_utils.h"
#include "debug.h"


static size_t curl_easy_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata);
static CURLcode curl_easy_post_json_request(CURL *curl_handle, const gchar *url, json_t *request, json_t **response);
static CURLcode curl_easy_get_json_request(CURL *curl_handle, const gchar *url, json_t **response);

static size_t curl_easy_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {

	json_error_t error;

	*((json_t **)userdata) = json_loads(ptr, 0, &error);

	return size*nmemb;
}

static CURLcode curl_easy_post_json_request(CURL *curl_handle, const gchar *url, json_t *request, json_t **response) {
	
	struct curl_slist *headers = NULL;
	gchar *request_text = NULL;
	CURLcode return_value = CURLE_OK;

	do {
		// allocation
		headers = curl_slist_append(headers, "Accept: application/json");
		if (!headers) {
			JANUS_LOG(LOG_ERR, "Could not append header.\n");
			return_value = CURLE_OUT_OF_MEMORY;
			break;
		}
		// allocation
		headers = curl_slist_append(headers, "Content-Type: application/json");
		if (!headers) {
			JANUS_LOG(LOG_ERR, "Could not append header.\n");
			return_value = CURLE_OUT_OF_MEMORY;
			break;
		}
		// allocation
		headers = curl_slist_append(headers, "charsets: utf-8");
		if (!headers) {
			JANUS_LOG(LOG_ERR, "Could not append header.\n");
			return_value = CURLE_OUT_OF_MEMORY;
			break;
		}		
		return_value = curl_easy_setopt(curl_handle, CURLOPT_URL, url);
		if (CURLE_OK != return_value) {
			JANUS_LOG(LOG_ERR, "Could not set CURLOPT_URL.\n");
			break;
		}
		return_value = curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);
		if (CURLE_OK != return_value) {
			JANUS_LOG(LOG_ERR, "Could not set CURLOPT_NOPROGRESS.\n");
			break;
		}
		return_value = curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 10L); 
		if (CURLE_OK != return_value) {
			JANUS_LOG(LOG_ERR, "Could not set CURLOPT_TIMEOUT.\n");
			break;
		}
		return_value = curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 0L); 	
		if (CURLE_OK != return_value) {
			JANUS_LOG(LOG_ERR, "Could not set CURLOPT_NOSIGNAL.\n");
			break;
		}
		return_value = curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
		if (CURLE_OK != return_value) {
			JANUS_LOG(LOG_ERR, "Could not set CURLOPT_HTTPHEADER.\n");
			break;
		}
		request_text = json_dumps(request, JSON_PRESERVE_ORDER);
		JANUS_LOG(LOG_VERB, "curl_easy_post_json_request %s\n", request_text);
		return_value = curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, request_text);		
		if (CURLE_OK != return_value) {
			JANUS_LOG(LOG_ERR, "Could not set CURLOPT_POSTFIELDS.\n");
			break;
		}
		return_value = curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, curl_easy_write_callback);
		if (CURLE_OK != return_value) {
			JANUS_LOG(LOG_ERR, "Could not set CURLOPT_WRITEFUNCTION.\n");
			break;
		}
		return_value = curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)response);
		if (CURLE_OK != return_value) {
			JANUS_LOG(LOG_ERR, "Could not set CURLOPT_WRITEDATA.\n");
			break;
		}
		return_value = curl_easy_perform(curl_handle);
		if (CURLE_OK != return_value) {
			JANUS_LOG(LOG_ERR, "Could not perform curl request.\n");
			break;		
		}
		curl_slist_free_all(headers);
		headers = NULL;	
		g_free(request_text);
		request_text = NULL;
	}
	while(0);

	// cleanup
	if (headers) {
		curl_slist_free_all(headers);
		headers = NULL;
	}	
	if (request_text) {
		g_free(request_text);
		request_text = NULL;
	}

	return return_value;
}

CURLcode curl_easy_get_json_request(CURL *curl_handle, const gchar *url, json_t **response) {

	CURLcode return_value = CURLE_OK;

	do {
		return_value = curl_easy_setopt(curl_handle, CURLOPT_URL, url);
		if (CURLE_OK != return_value) {
			JANUS_LOG(LOG_ERR, "Could not set CURLOPT_URL.\n");
			break;
		}
		return_value = curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, curl_easy_write_callback);
		if (CURLE_OK != return_value) {
			JANUS_LOG(LOG_ERR, "Could not set CURLOPT_WRITEFUNCTION.\n");
			break;
		}
		return_value = curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)response);
		if (CURLE_OK != return_value) {
			JANUS_LOG(LOG_ERR, "Could not set CURLOPT_WRITEDATA.\n");
			break;
		}
		return_value = curl_easy_perform(curl_handle);
		if (CURLE_OK != return_value) {
			JANUS_LOG(LOG_ERR, "Could not perform curl request.\n");
			break;		
		}
	}
	while(0);

	return return_value;
}

json_t *json_registry_source_request(const gchar *url) {

	CURL *curl_handle = NULL;
	json_t *json_object_response = NULL;
	CURLcode return_value = CURLE_OK;

	do
	{
		// allocation
		curl_handle = curl_easy_init();
		if (!curl_handle) {
			JANUS_LOG(LOG_ERR, "Could not initialize curl.\n");
			break;
		}
		return_value = curl_easy_get_json_request(curl_handle, url, &json_object_response);
		if (CURLE_OK != return_value) {
			JANUS_LOG(LOG_ERR, "Could not get registry.\n");
			break;
		}
		curl_easy_cleanup(curl_handle);
		curl_handle = NULL;
	}
	while(0);

	if (curl_handle) {
		curl_easy_cleanup(curl_handle);
		curl_handle = NULL;
	}

	return json_object_response;
}

gchar *get_source_from_registry_by_id(const gchar *registry_url, const gchar *id) {

	gchar *url = NULL;
	json_t *json_source = NULL;
	gchar *source = NULL;

	do {
		if (!registry_url) {
			JANUS_LOG(LOG_ERR, "Registry url not specified.\n");
			break;
		}
		// allocation
		url = g_strdup_printf("%s/?id=%s", registry_url, id);
		// allocation
		json_source = json_registry_source_request(url);
		g_free(url);
		url = NULL;
		if (!json_is_array(json_source)) {
			JANUS_LOG(LOG_ERR, "Not valid json array.\n");
			break;
		}
		json_t *json_object = json_array_get(json_source, 0);
		if (!json_is_object(json_object)) {
			JANUS_LOG(LOG_ERR, "Not valid json object.\n");
			break;
		}		
		json_t *json_id = json_object_get(json_object, "id");
		if (!json_is_string(json_id)) {
			JANUS_LOG(LOG_ERR, "id is not a string.\n");
			break;
		}
		json_t *json_uri = json_object_get(json_object, "uri");
		if (!json_is_string(json_uri)) {
			JANUS_LOG(LOG_ERR, "uri is not a string.\n");
			break;
		}
		// allocation
		source = g_strdup(json_string_value(json_uri));
		json_decref(json_source);
		json_source = NULL;
	}
	while(0);

	if (url) {
		g_free(url);
		url = NULL;
	}
	if (json_source) {
		json_decref(json_source);
		json_source = NULL;
	}
	return source;
}



