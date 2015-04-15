/*
 * Copyright (C) 2013 Igalia S.L.
 *
 * Author: Juan A. Suarez Romero <jasuarez@igalia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <grilo.h>

#include <gst/pbutils/pbutils.h>

#define GSTREAMER_ID "grl-gstreamer"
#define FILESYSTEM_ID "grl-filesystem"

static void
test_setup (void)
{
  GError *error = NULL;
  GrlRegistry *registry;

  registry = grl_registry_get_default ();

  grl_registry_load_all_plugins (registry, &error);
  g_assert_no_error (error);
}

static void
test_discover_local (void)
{
  GError *error = NULL;
  GrlOperationOptions *options;
  GrlRegistry *registry = grl_registry_get_default();
  GrlSource *source;
  GrlMedia *media;
  gchar *uri;
  GstDiscovererInfo *info;
  GList *streams;
  const GValue *info_value;
  GrlKeyID disco_key_id = grl_registry_lookup_metadata_key(registry, "discovery");
  GList * keys = grl_metadata_key_list_new (GRL_METADATA_KEY_TITLE,
      GRL_METADATA_KEY_EXTERNAL_URL,
      GRL_METADATA_KEY_URL,
      disco_key_id,
      GRL_METADATA_KEY_INVALID);

  g_assert (disco_key_id != GRL_METADATA_KEY_INVALID);
  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, FILESYSTEM_ID);
  g_assert (source);
  options = grl_operation_options_new (NULL);
  grl_operation_options_set_count (options, 2);
  grl_operation_options_set_resolution_flags (options, GRL_RESOLVE_FULL);
  g_assert (options);

  uri = g_filename_to_uri (GSTREAMER_DATA_PATH "/theora-vorbis.ogg", NULL, &error);
  g_assert (error == NULL);
  media = grl_source_get_media_from_uri_sync (source,
      uri,
      keys,
      options,
      &error);

  g_assert (media);

  info_value = grl_data_get (GRL_DATA (media), disco_key_id);

  g_assert (info_value);

  info = g_value_get_object (info_value);

  g_assert (GST_IS_DISCOVERER_INFO (info));

  streams = gst_discoverer_info_get_audio_streams (info);
  g_assert (g_list_length (streams) == 1);
  gst_discoverer_stream_info_list_free (streams);

  g_list_free (keys);
  g_free (uri);
  g_object_unref (options);
  g_object_unref (media);
  gst_discoverer_info_unref (info);
}

int
main (int argc, char **argv)
{
  g_setenv ("GRL_PLUGIN_PATH", FILESYSTEM_PLUGIN_PATH ":" GSTREAMER_PLUGIN_PATH, TRUE);
  g_setenv ("GRL_PLUGIN_LIST", FILESYSTEM_ID ":" GSTREAMER_ID, TRUE);

  grl_init (&argc, &argv);
  g_test_init (&argc, &argv, NULL);

#if !GLIB_CHECK_VERSION(2,32,0)
  g_thread_init (NULL);
#endif

  test_setup ();

  g_test_add_func ("/gstreamer/discover/local", test_discover_local);

  gint result = g_test_run ();

  grl_deinit ();
  return result;
}
