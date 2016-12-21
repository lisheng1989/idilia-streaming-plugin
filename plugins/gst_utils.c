
#include "debug.h"
#include "socket_utils.h"
#include "ports_pool.h"
#include "curl_utils.h"
#include <gst/gst.h>
#include <gst/sdp/gstsdpmessage.h>  
#include <gst/rtsp/rtsp.h>
#include "gst_utils.h"
#include "idilia_streaming.h"


static GstElement * 
create_remote_rtcp_input(const gchar * media, GSocket *socket);
static void sender_bin_add_media_pads_to_rtpbin(GstElement * bin, guint pad_id, const gchar * media);


static void link_rtp_pad_to_sender_bin(GstElement * source, GstPad * input_pad, const gchar *media, pipeline_callback_t * callback_data);

static void
rtspsrc_on_no_more_pads (GstElement *element, pipeline_callback_t * callback_data);


static void
rtspsrc_pad_added_callback(GstElement * element, GstPad * pad, pipeline_callback_t * callback_data);


static gboolean print_field (GQuark field, const GValue * value, gpointer pfx)
{
  gchar *str = gst_value_serialize (value);
  
  JANUS_LOG (LOG_VERB, "  %15s: %s\n" , g_quark_to_string (field), str);
  g_free (str);
  return TRUE;
}

gboolean on_eos(GstBus *bus, GstMessage *message, gpointer data)
{
	JANUS_LOG(LOG_INFO, "Handling pipeline EOS event\n");

    pipeline_callback_t * callback_data = (pipeline_callback_t*)data;

    if (!callback_data) {
        JANUS_LOG(LOG_ERR, "Callback data is NULL!");
        return;
    }

    janus_streaming_send_destroy_request(callback_data->mountpoint->id, callback_data->handle);

	return TRUE;
}

gboolean on_error(GstBus *bus, GstMessage *message, gpointer data)
{
	GError *error = NULL;
	gchar *debug = NULL;

	gst_message_parse_error(message, &error, &debug);
	JANUS_LOG(LOG_ERR, "Error: %s\n", error->message ? error->message : "??");
	JANUS_LOG(LOG_DBG, "Debugging info: %s\n", debug ? debug : "??");
	g_error_free(error);
	g_free(debug);
	on_eos(bus, message, data);
	return TRUE;
}

GstElement * 
create_remote_rtp_output(guint port, const gchar * media)
{
    GstElement * udpsink, *filter;	
	GstElement *sinkbin;
	GstPad * pad;
	GstCaps *filtercaps;

	sinkbin = gst_bin_new (NULL);
  
    gchar *name = g_strdup_printf("rtp_sink_%s", media);
	
    udpsink = gst_element_factory_make ("udpsink", name);
    g_assert (udpsink);
    g_object_set (G_OBJECT (udpsink), "port", port, NULL);
	
	filter = gst_element_factory_make ("capsfilter", "filter");
	g_assert (filter != NULL); 

	filtercaps = gst_caps_new_simple ( "application/x-rtp", "media", G_TYPE_STRING, media, NULL);
	g_object_set (G_OBJECT (filter), "caps", filtercaps, NULL);

	gst_bin_add_many (GST_BIN (sinkbin),  filter, udpsink, NULL);
	gst_element_link (filter, udpsink);

	pad = gst_element_get_static_pad (filter, "sink");
	g_assert (pad);

	if(gst_element_add_pad (GST_ELEMENT(sinkbin), gst_ghost_pad_new ("sink", pad)) != TRUE){
		JANUS_LOG(LOG_INFO,"gst_element_add_pad failed \n");		
	}

	gst_object_unref (GST_OBJECT (pad));
    
    g_free(name);

	return GST_ELEMENT(sinkbin);
}

static GstElement * 
create_remote_rtcp_input(const gchar * media, GSocket *socket)
{
    GstElement * udpsrc;
    gchar *name = g_strdup_printf("rtcp_src_%s", media);

    udpsrc = gst_element_factory_make ("udpsrc", name);
    g_assert (udpsrc);
    g_object_set(udpsrc, "socket", socket, NULL);
	g_object_set(udpsrc, "close-socket", FALSE, NULL);	
    g_free(name);

    return udpsrc;
}

static void sender_bin_add_media_pads_to_rtpbin(GstElement * bin, guint pad_id, const gchar * media)
{
    GstElement * rtpbin;
    GstPad *rtpbin_send_rtp_sink_pad, *rtpbin_send_rtp_src_pad, *rtpbin_recv_rctp_sink_pad, *ghost_pad;

    gchar * sinkpad_name, *srcpad_name, *rtcp_sinkpad_name, * rtpbin_sinkpad_name, *rtpbin_srcpad_name, *rtpbin_recv_rtcp_sink_pad_name;

    sinkpad_name = g_strdup_printf("%s_sink", media);
    srcpad_name = g_strdup_printf("%s_src", media);
    rtcp_sinkpad_name = g_strdup_printf("%s_rtcp_sink", media);
    rtpbin_sinkpad_name = g_strdup_printf("send_rtp_sink_%u", pad_id);
    rtpbin_srcpad_name = g_strdup_printf("send_rtp_src_%u", pad_id);
    rtpbin_recv_rtcp_sink_pad_name = g_strdup_printf("recv_rtcp_sink_%u", pad_id);

    rtpbin = gst_bin_get_by_name(GST_BIN(bin), "rtpbin");
    g_assert(rtpbin);

    rtpbin_send_rtp_sink_pad = gst_element_get_request_pad (rtpbin, rtpbin_sinkpad_name);
    g_assert(rtpbin_send_rtp_sink_pad);
	ghost_pad = gst_ghost_pad_new (sinkpad_name, rtpbin_send_rtp_sink_pad);
	gst_pad_set_active(ghost_pad, TRUE);
    gst_element_add_pad (bin, ghost_pad);
    gst_object_unref (rtpbin_send_rtp_sink_pad);
	
    rtpbin_send_rtp_src_pad = gst_element_get_static_pad (rtpbin, rtpbin_srcpad_name);
    g_assert(rtpbin_send_rtp_src_pad);
	ghost_pad = gst_ghost_pad_new (srcpad_name, rtpbin_send_rtp_src_pad);
	gst_pad_set_active(ghost_pad, TRUE);
    gst_element_add_pad (bin, ghost_pad);
    gst_object_unref (rtpbin_send_rtp_src_pad);

    rtpbin_recv_rctp_sink_pad = gst_element_get_request_pad (rtpbin, rtpbin_recv_rtcp_sink_pad_name);
    g_assert(rtpbin_recv_rctp_sink_pad);
	ghost_pad = gst_ghost_pad_new (rtcp_sinkpad_name, rtpbin_recv_rctp_sink_pad);
	gst_pad_set_active(ghost_pad, TRUE);
    gst_element_add_pad (bin, ghost_pad);
    gst_object_unref (rtpbin_recv_rctp_sink_pad);

    g_free(sinkpad_name);
    g_free(srcpad_name);
    g_free(rtpbin_sinkpad_name);
    g_free(rtpbin_srcpad_name);   
    g_free(rtcp_sinkpad_name);
    g_free(rtpbin_recv_rtcp_sink_pad_name);   

}

GstElement * sender_bin_create (void)
{
    GstElement *bin, *rtpbin;

    bin = gst_bin_new ("sender_bin");
    g_assert (bin);

    rtpbin = gst_element_factory_make ("rtpbin", "rtpbin");
    g_assert (rtpbin);

	
    g_object_set (G_OBJECT (rtpbin), "rtp-profile", 3, NULL);
    g_object_set (G_OBJECT (rtpbin), "latency",  0, NULL);
	
    gst_bin_add_many (GST_BIN (bin), rtpbin, NULL);


    return bin;
}

static void link_rtp_pad_to_sender_bin(GstElement * source, GstPad * input_pad, const gchar *media, pipeline_callback_t * callback_data)
{

    GstElement *udp_sink_bin = NULL, * sender_bin, *rtcp_src;
    gchar *rtcp_sinkpad_name;
    GstElement * pipeline = callback_data->pipeline;
    gint stream_type;
    GstPad *rtcp_srcpad, *senderbin_rtcp_sinkpad, *rtp_bin_sink_pad;

    g_assert(input_pad);
    g_assert(pipeline);
    g_assert(media);

    sender_bin = gst_bin_get_by_name(GST_BIN(pipeline), "sender_bin");
    g_assert(sender_bin);

    if (!g_strcmp0 (media, "audio")) {
	  sender_bin_add_media_pads_to_rtpbin(sender_bin, 1, media);
      stream_type = JANUS_STREAMING_STREAM_AUDIO;
    } else if (!g_strcmp0 (media, "video")) {
	  sender_bin_add_media_pads_to_rtpbin(sender_bin, 0, media);
      stream_type = JANUS_STREAMING_STREAM_VIDEO;
    } else {
      return;
    }

    udp_sink_bin = create_remote_rtp_output(callback_data->mountpoint->socket[stream_type][JANUS_STREAMING_SOCKET_RTP_SRV].port, media);
    g_assert(udp_sink_bin);

    rtcp_src = create_remote_rtcp_input(media, callback_data->mountpoint->socket[stream_type][JANUS_STREAMING_SOCKET_RTCP_RCV_SRV].socket);
    g_assert(rtcp_src);

    gst_bin_add_many (GST_BIN (pipeline), udp_sink_bin, rtcp_src, NULL);

    g_assert (gst_element_set_state (udp_sink_bin, GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE);

    rtcp_sinkpad_name = g_strdup_printf("%s_rtcp_sink", media);

    senderbin_rtcp_sinkpad = gst_element_get_static_pad (sender_bin, rtcp_sinkpad_name);
    g_assert(senderbin_rtcp_sinkpad);
  
    rtcp_srcpad = gst_element_get_static_pad (rtcp_src, "src");
    g_assert(rtcp_srcpad);

    g_assert (gst_pad_link (rtcp_srcpad, senderbin_rtcp_sinkpad) == GST_PAD_LINK_OK);

    gst_element_link_many (source, sender_bin, udp_sink_bin, NULL);	

    rtp_bin_sink_pad = gst_element_get_static_pad (udp_sink_bin, "sink");
    g_assert(rtp_bin_sink_pad);
	gst_object_unref(rtp_bin_sink_pad);


    g_free(rtcp_sinkpad_name);
	gst_object_unref(senderbin_rtcp_sinkpad);
	gst_object_unref(rtcp_srcpad);
	gst_object_unref(sender_bin);

	return;
}

static void
rtspsrc_on_no_more_pads (GstElement *element, pipeline_callback_t * callback_data)
{
    GstElement * pipeline = callback_data->pipeline;
    g_assert(GST_IS_PIPELINE(pipeline));

    if (!callback_data) {
        JANUS_LOG(LOG_ERR, "Callback data is NULL!");
        return;
    }
		
    janus_streaming_send_watch_request(callback_data->mountpoint->id, callback_data->handle);
}


static void
rtspsrc_pad_added_callback(GstElement * element, GstPad * pad, pipeline_callback_t * callback_data)
{
    GstCaps *caps;
    GstStructure *s;
    const gchar *media;
    const gchar *type;	
	const gchar *encoding_name;
	gint payload;
	gint encoding_params;
	gint clock_rate;
    gboolean connect_output = FALSE;
    GstElement * pipeline = callback_data->pipeline;

    g_assert(GST_IS_PIPELINE(pipeline));

    caps = gst_pad_get_current_caps (pad);

    if (caps != NULL) {
        s = gst_caps_get_structure (caps, 0);	
		
		gst_structure_get_int (s, "clock-rate", &clock_rate);
		gst_structure_get_int(s,"payload",&payload);
		encoding_name = gst_structure_get_string(s,"encoding-name");	
		gst_structure_get_int(s,"encoding-params",&encoding_params);		
		gst_structure_foreach(s,print_field,NULL);

        type = gst_structure_get_name(s);

        if (!g_strcmp0 (type, "application/x-rtp")) {
            media = gst_structure_get_string (s, "media");
			 
            if (!g_strcmp0 (media, "audio")) {				
				callback_data->mountpoint->codecs.audio_pt = payload;
				callback_data->mountpoint->codecs.isAudio = TRUE;	
				callback_data->mountpoint->codecs.audio_rtpmap = g_strdup_printf ("%d %s/%d",payload, encoding_name,clock_rate);			 								
                connect_output = TRUE;
            } else if (!g_strcmp0 (media, "video")) {
				callback_data->mountpoint->codecs.isVideo = TRUE;				
				callback_data->mountpoint->codecs.video_pt = payload;
				callback_data->mountpoint->codecs.video_rtpmap = g_strdup_printf ("%d %s/%d", payload, encoding_name,clock_rate);									
                connect_output = TRUE;				
            } else {
                JANUS_LOG (LOG_WARN, "Unknown media type: %s\n", media);
            }
        } else if (!g_strcmp0 (type, "application/x-rtcp")) {

        } else {
			JANUS_LOG (LOG_WARN, "Unknown type: %s\n", type);
        }
   
        gst_caps_unref (caps);	
    }

    if (connect_output) {
		JANUS_LOG (LOG_INFO, "rtspsrc_pad_added_callback\n");
        link_rtp_pad_to_sender_bin(element, pad, media, callback_data);
    }
}

static void
rtspsrc_rtpbin_on_timeout (GstElement *sess, guint ssrc, pipeline_callback_t * callback_data)
{
	GstEvent *eos = gst_event_new_eos();
	JANUS_LOG(LOG_INFO, "RTSPsrc timeout occured, sending EOS\n");
	gst_element_send_event (sess, eos);
}


static void
rtspsrc_rtpbin_on_bye_ssrc (GstElement *sess, guint ssrc, pipeline_callback_t * callback_data)
{	
	JANUS_LOG(LOG_ERR, "RTSPsrc BYE received, sending EOS\n");
	//GstEvent *eos = gst_event_new_eos();
	//gst_element_send_event (sess, eos);
}


static void
rtspsrc_rtpbin_on_new_ssrc (GstElement *rtpbin, guint session_id, guint ssrc, pipeline_callback_t * callback_data)
{
	GstElement  *session;
	g_signal_emit_by_name (rtpbin, "get-session", session_id, &session);
	g_assert(session);
	g_signal_connect(session, "on-timeout",  (GCallback) rtspsrc_rtpbin_on_timeout,  callback_data);
	g_signal_connect(session, "on-bye-ssrc", (GCallback) rtspsrc_rtpbin_on_bye_ssrc, callback_data);
	g_object_unref (session);
	
}

static void
rtspsrc_on_new_manager(GstElement * rtspsrc, GstElement * rtpbin, pipeline_callback_t * callback_data)
{
	g_assert(rtpbin);
  	g_signal_connect(rtpbin, "on-new-ssrc",  (GCallback) rtspsrc_rtpbin_on_new_ssrc, callback_data);
}


static void
rtspsrc_on_handle_request (GstElement *rtspsrc, GstRTSPMessage *request, GstRTSPMessage *response, pipeline_callback_t * callback_data)
{
	const gchar * uri = NULL;
	GstRTSPMethod method = GST_RTSP_INVALID;
	GstRTSPResult res;
	
	res = gst_rtsp_message_parse_request (request, &method, &uri, NULL);

	if (res == GST_RTSP_OK) {
		
		JANUS_LOG(LOG_INFO, "rtspsrc_on_handle_request: %d\n", method);

		if (method == GST_RTSP_TEARDOWN) {
			if (g_strcmp0(uri, callback_data->uri) == 0) {
				JANUS_LOG(LOG_INFO, "Received TEARDOWN for %s, sending EOS\n", uri);
				GstEvent *eos = gst_event_new_eos();
				gst_element_send_event (rtspsrc, eos);
			} else {
				JANUS_LOG(LOG_WARN, "Received TEARDOWN for unknown url: %s\n", uri);
			}

		} else {
			JANUS_LOG(LOG_WARN, "rtspsrc_on_handle_request unknown method: %d\n", method);
		}
	}
}

GstElement * create_rtsp_source_element(gpointer user_data, const pipeline_data_t * pipeline_data)
{
  GstElement * source = NULL;

  source = gst_element_factory_make ("rtspsrc", "source");
  g_assert (source);
  g_object_set (G_OBJECT (source), 
      "location", pipeline_data->uri,
      "latency",  pipeline_data->latency, 
      "async-handling", TRUE, NULL);
  

  g_signal_connect(source, "pad-added",      (GCallback) rtspsrc_pad_added_callback, user_data);
  g_signal_connect(source, "no-more-pads",   (GCallback) rtspsrc_on_no_more_pads,    user_data);
  g_signal_connect(source, "new-manager",    (GCallback) rtspsrc_on_new_manager,     user_data);
  g_signal_connect(source, "handle-request", (GCallback) rtspsrc_on_handle_request,  user_data);

  return source;
}

//todo: fill codecs params in the mountpoint
GstElement *
create_videotestsrc_bin (gpointer user_data, const pipeline_data_t * pipeline_data)
{

  GstElement *bin, *source, *converter, *encoder, *payloader;
  GstPad *pad;

  bin = gst_bin_new ("videotestsrcbin");
  g_assert (bin);

  source = gst_element_factory_make ("videotestsrc", "source");
  g_assert (source);

  converter = gst_element_factory_make ("videoconvert", "converter");
  g_assert (converter);

  encoder = gst_element_factory_make ("vp8enc", "encoder");
  g_assert (encoder);

  payloader = gst_element_factory_make ("rtpvp8pay", "payloader");
  g_assert (payloader);

  g_object_set (G_OBJECT (payloader), 
      "pt", 96,
      NULL);
  	  

  gst_bin_add_many (GST_BIN (bin), source, converter, encoder, payloader, NULL);

  gst_element_link_many (source, converter, encoder, payloader, NULL);

  pad = gst_element_get_static_pad (payloader, "src");
  gst_element_add_pad (bin, gst_ghost_pad_new ("src", pad));
  gst_object_unref (pad);

  return bin;
}
