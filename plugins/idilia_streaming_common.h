#pragma once

#include <gst/gst.h>
#include "socket_utils.h"
#include "../mutex.h"


enum
{
	JANUS_STREAMING_STREAM_VIDEO = 0,
	JANUS_STREAMING_STREAM_AUDIO,
	JANUS_STREAMING_STREAM_MAX
};

enum
{
	JANUS_STREAMING_SOCKET_RTP_SRV = 0,
	JANUS_STREAMING_SOCKET_RTCP_RCV_SRV,
	JANUS_STREAMING_SOCKET_RTCP_RCV_CLI,
	JANUS_STREAMING_SOCKET_RTCP_SND_SRV,
	JANUS_STREAMING_SOCKET_MAX
};
  
typedef struct socket_callback_data
{
	gpointer * session;
	gboolean is_video;
} janus_streaming_socket_cbk_data;

typedef struct janus_streaming_codecs {
	gint audio_pt;
	char *audio_rtpmap;
	char *audio_fmtp;
	gint video_codec;
	gint video_pt;
	char *video_rtpmap;
	char *video_fmtp;
	gboolean isAudio;
	gboolean isVideo;
} janus_streaming_codecs;


typedef struct janus_streaming_mountpoint {
	gchar *id;
	char *name;
	char *description;
	gboolean is_private;
	char *secret;
	char *pin;
	gboolean enabled;
	gboolean active;	
	//void *source;	/* Can differ according to the source type */
	GDestroyNotify source_destroy;
	janus_streaming_codecs codecs;
	GList/*<unowned janus_streaming_session>*/ *listeners;
	gint64 destroyed;
	janus_mutex mutex;
	socket_utils_socket socket[JANUS_STREAMING_STREAM_MAX][JANUS_STREAMING_SOCKET_MAX];
	janus_streaming_socket_cbk_data rtp_cbk_data[JANUS_STREAMING_STREAM_MAX];
	guint32 ssrc[JANUS_STREAMING_STREAM_MAX];
	GstCaps *caps;
} janus_streaming_mountpoint;

