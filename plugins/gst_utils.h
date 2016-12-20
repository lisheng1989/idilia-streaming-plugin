#pragma once

#include <gst/gst.h>
#include "idilia_streaming_common.h"
#include "plugin.h"

typedef struct {

	gchar *id;
	guint latency;
	gchar *uri;
	janus_plugin_session *handle;
} pipeline_data_t;

typedef struct {
	janus_streaming_mountpoint * mountpoint;
	janus_plugin_session *handle;
	GstElement * pipeline;
	const gchar *uri;
} pipeline_callback_t;


gboolean on_eos(GstBus *bus, GstMessage *message, gpointer data);
gboolean on_error(GstBus *bus, GstMessage *message, gpointer data);
GstElement * sender_bin_create(void);
GstElement * create_rtsp_source_element(gpointer user_data, const pipeline_data_t * pipeline_data);
GstElement *
create_videotestsrc_bin (gpointer user_data, const pipeline_data_t * pipeline_data);
GstElement * 
create_remote_rtp_output(guint port, const gchar * media);
