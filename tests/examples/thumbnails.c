/* GStreamer Editing Services
 * Copyright (C) 2010 Edward Hervey <bilboed@bilboed.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <ges/ges.h>
#include <gst/pbutils/encoding-profile.h>

/* GLOBAL VARIABLE */
static guint repeat = 0;
GESTimelinePipeline *pipeline = NULL;

static gboolean thumbnail_cb (gpointer pipeline);

#define TEST_PATH "test_thumbnail.jpg"

static gboolean
thumbnail_cb (gpointer user)
{
  GstBuffer *b = NULL;
  GstCaps *caps;
  GESTimelinePipeline *p;

  p = GES_TIMELINE_PIPELINE (user);

  caps = gst_caps_from_string ("image/jpeg");
  GST_INFO ("getting thumbnails");

  /* check raw rgb use-case with scaling */
  b = ges_timeline_pipeline_get_thumbnail_rgb24 (p, 320, 240);
  g_assert (b);
  gst_buffer_unref (b);

  /* check encoding use-case from caps */
  b = NULL;
  b = ges_timeline_pipeline_get_thumbnail_buffer (p, caps);
  g_assert (b);
  gst_buffer_unref (b);

  g_assert (ges_timeline_pipeline_save_thumbnail (p, -1, -1, (gchar *)
          "image/jpeg", (gchar *) TEST_PATH));
  g_assert (g_file_test (TEST_PATH, G_FILE_TEST_EXISTS));
  g_unlink (TEST_PATH);

  gst_caps_unref (caps);
  return FALSE;
}

static GESTimelinePipeline *
create_timeline (void)
{
  GESTimelinePipeline *pipeline;
  GESTimelineLayer *layer;
  GESTrack *tracka, *trackv;
  GESTimeline *timeline;
  GESTimelineObject *src;

  timeline = ges_timeline_new ();

  tracka = ges_track_audio_raw_new ();
  trackv = ges_track_video_raw_new ();

  layer = (GESTimelineLayer *) ges_simple_timeline_layer_new ();

  /* Add the tracks and the layer to the timeline */
  if (!ges_timeline_add_layer (timeline, layer) ||
      !ges_timeline_add_track (timeline, tracka) ||
      !ges_timeline_add_track (timeline, trackv))
    return NULL;

  /* Add the main audio/video file */
  src = GES_TIMELINE_OBJECT (ges_timeline_test_source_new ());
  g_object_set (src,
      "vpattern", GES_VIDEO_TEST_PATTERN_SNOW,
      "duration", 10 * GST_SECOND, NULL);

  ges_simple_timeline_layer_add_object ((GESSimpleTimelineLayer *) layer,
      GES_TIMELINE_OBJECT (src), 0);

  pipeline = ges_timeline_pipeline_new ();

  if (!ges_timeline_pipeline_add_timeline (pipeline, timeline))
    return NULL;

  return pipeline;
}

static void
bus_message_cb (GstBus * bus, GstMessage * message, GMainLoop * mainloop)
{
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:
      g_print ("ERROR\n");
      g_main_loop_quit (mainloop);
      break;
    case GST_MESSAGE_EOS:
      if (repeat > 0) {
        g_print ("Looping again\n");
        /* No need to change state before */
        gst_element_seek_simple (GST_ELEMENT (pipeline), GST_FORMAT_TIME,
            GST_SEEK_FLAG_FLUSH, 0);
        gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PLAYING);
        repeat -= 1;
      } else {
        g_print ("Done\n");
        g_main_loop_quit (mainloop);
      }
      break;
    default:
      break;
  }
}

int
main (int argc, gchar ** argv)
{
  GError *err = NULL;
  GOptionEntry options[] = {
    {NULL}
  };
  GOptionContext *ctx;
  GMainLoop *mainloop;
  GstBus *bus;

  if (!g_thread_supported ())
    g_thread_init (NULL);

  ctx = g_option_context_new ("tests thumbnail supoprt (produces no output)");
  g_option_context_set_summary (ctx, "");
  g_option_context_add_main_entries (ctx, options, NULL);
  g_option_context_add_group (ctx, gst_init_get_option_group ());

  if (!g_option_context_parse (ctx, &argc, &argv, &err)) {
    g_print ("Error initializing: %s\n", err->message);
    g_option_context_free (ctx);
    exit (1);
  }

  g_option_context_free (ctx);
  /* Initialize the GStreamer Editing Services */
  ges_init ();

  /* Create the pipeline */
  pipeline = create_timeline ();
  if (!pipeline)
    exit (-1);

  ges_timeline_pipeline_set_mode (pipeline, TIMELINE_MODE_PREVIEW);

  /* Play the pipeline */
  mainloop = g_main_loop_new (NULL, FALSE);

  g_print ("thumbnailing every 1 seconds\n");
  g_timeout_add (1000, thumbnail_cb, pipeline);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (bus_message_cb), mainloop);

  if (gst_element_set_state (GST_ELEMENT (pipeline),
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
    g_print ("Failed to start the encoding\n");
    return 1;
  }
  g_main_loop_run (mainloop);

  gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_NULL);
  gst_object_unref (pipeline);

  return 0;
}
