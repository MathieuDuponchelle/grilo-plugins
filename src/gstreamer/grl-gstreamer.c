/*
 * Copyright (C) 2015 Mathieu Duponchelle <mathieu.duponchelle@collabora.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <grilo.h>
#include <gio/gio.h>
#include <glib/gi18n-lib.h>

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include <gom/gom.h>

#include "discovery-info-resource.h"

#include "grl-gstreamer.h"

#define GRL_SQL_DB "grl-gstreamer.db"

#define GRL_LOG_DOMAIN_DEFAULT gstreamer_log_domain
GRL_LOG_DOMAIN_STATIC (gstreamer_log_domain);

#define PLUGIN_ID   GSTREAMER_PLUGIN_ID

#define SOURCE_ID   "grl-gstreamer"
#define SOURCE_NAME _("GStreamer Discoveries Provider")
#define SOURCE_DESC _("A source providing results from gst-discoverer")

G_DEFINE_TYPE (GrlGStreamerSource, grl_gstreamer_source, GRL_TYPE_SOURCE);

#define GRL_GSTREAMER_SOURCE_GET_PRIVATE(object)           \
  (G_TYPE_INSTANCE_GET_PRIVATE((object),                        \
                               GRL_GSTREAMER_SOURCE_TYPE,	\
                               GrlGStreamerSourcePriv))

static GrlKeyID GRL_GSTREAMER_METADATA_KEY_DISCOVERY = GRL_METADATA_KEY_INVALID;

enum
{
  PROP_0,
  PROP_TIMEOUT,
};

struct _GrlGStreamerSourcePriv
{
  GstDiscoverer *discoverer;
  GHashTable *resolve_specs_by_uri;

  gint timeout;
  GomAdapter *adapter;
  GomRepository *repository;
};

static GrlGStreamerSource *grl_gstreamer_source_new (gint timeout);

gboolean grl_gstreamer_source_plugin_init (GrlRegistry * registry,
    GrlPlugin * plugin, GList * configs);

/* =================== GrlGStreamer Plugin  =============== */

static GrlGStreamerSource *
grl_gstreamer_source_new (gint timeout)
{
  GRL_DEBUG ("grl_gstreamer_source_new");
  return g_object_new (GRL_GSTREAMER_SOURCE_TYPE,
      "source-id", SOURCE_ID,
      "source-name", SOURCE_NAME, "source-desc", SOURCE_DESC,
      "discoverer-timeout", timeout, NULL);
}

static void
register_key (GrlRegistry * registry)
{
  GParamSpec *spec;

  spec = g_param_spec_object ("discovery",
      "discovery",
      "The gstreamer discovery of a media, as a #GstDiscovererInfo",
      GST_TYPE_DISCOVERER_INFO, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  GRL_GSTREAMER_METADATA_KEY_DISCOVERY =
      grl_registry_register_metadata_key (registry, spec, NULL);
}

gboolean
grl_gstreamer_source_plugin_init (GrlRegistry * registry,
    GrlPlugin * plugin, GList * configs)
{
  GrlConfig *config;
  gint config_count;
  gint timeout = 1;

  GRL_LOG_DOMAIN_INIT (gstreamer_log_domain, "gstreamer");

  GRL_DEBUG ("grl_gstreamer_source_plugin_init");

  gst_init (NULL, NULL);
  /* Initialize i18n */
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  register_key (registry);

  config_count = g_list_length (configs);

  if (config_count >= 1) {
    config = GRL_CONFIG (configs->data);
    if (grl_config_has_param (config, "discoverer-timeout"))
      timeout = grl_config_get_int (config, "discoverer-timeout");
  }

  GrlGStreamerSource *source = grl_gstreamer_source_new (timeout);
  grl_registry_register_source (registry, plugin, GRL_SOURCE (source), NULL);
  return TRUE;
}

GRL_PLUGIN_REGISTER (grl_gstreamer_source_plugin_init, NULL, PLUGIN_ID);

/* ================== GrlGStreamer GObject ================ */

static gboolean
grl_gstreamer_source_may_resolve (GrlSource * source,
    GrlMedia * media, GrlKeyID key_id, GList ** missing_keys)
{
  if (!(key_id == GRL_GSTREAMER_METADATA_KEY_DISCOVERY))
    return FALSE;

  return TRUE;
}

static gchar *
make_key_for_media (GrlMedia *media)
{
  gchar *key = NULL;

  if (grl_media_get_external_url (media)) {
    key = g_strdup (grl_media_get_external_url (media));
  } else if (grl_media_get_url (media) && grl_media_get_modification_date (media)) {
    GDateTime *dt = grl_media_get_modification_date (media);
    GstDateTime *gstdt = gst_date_time_new_from_g_date_time (g_date_time_ref(dt));
    gchar *isodate = gst_date_time_to_iso8601_string (gstdt);
    key = g_strconcat (grl_media_get_url (media), isodate, NULL);
    g_free (isodate);
    gst_date_time_unref (gstdt);
  }

  return key;
}

static GomResource *
find_resource (GrlMedia *media, GomRepository * repository)
{
  GomResource *resource;
  GomFilter *filter;
  GValue value = { 0, };
  gchar *key = make_key_for_media (media);

  if (!key)
    return NULL;

  GRL_DEBUG ("%s looking for resource with key %s", __func__, key);

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, key);
  filter = gom_filter_new_eq (DISCOVERY_INFO_TYPE_RESOURCE, "key", &value);
  g_value_unset (&value);

  resource = gom_repository_find_one_sync (repository,
      DISCOVERY_INFO_TYPE_RESOURCE, filter, NULL);
  g_object_unref (filter);

  g_free (key);
  return resource;
}


static GomResource *
store_discovery (GrlGStreamerSource * source, GrlMedia *media,
    GstDiscovererInfo * info, GError ** error)
{
  GomResource *resource;
  gchar *key = make_key_for_media (media);

  if (!key) {
    const gchar *uri = gst_discoverer_info_get_uri (info);

    GRL_WARNING ("%s, can't make a key for resource %s\n", __func__,
        uri);

    *error =
        g_error_new (GRL_CORE_ERROR, GRL_CORE_ERROR_STORE_FAILED,
        "can't make a resource for %s", uri);

    return NULL;
  }

  resource = g_object_new (DISCOVERY_INFO_TYPE_RESOURCE,
      "repository", source->priv->repository, "key", key, NULL);

  g_object_set (resource, "info", info, NULL);

  if (!gom_resource_save_sync (resource, error)) {
    GRL_WARNING ("%s, got an error trying to save resource for key %s : %s\n", __func__,
        key, (*error)->message);
    g_object_unref (resource);
    resource = NULL;
  } else {
    GRL_DEBUG ("%s, stored discovery for key %s\n", __func__,
        key);
  }

  return resource;
}

static void
resolve_generic_discovery (GrlSourceResolveSpec * rs, GomResource * resource)
{
  GList *tmp;

  for (tmp = rs->keys; tmp; tmp = tmp->next) {
    GrlKeyID key = GRLPOINTER_TO_KEYID (tmp->data);
    if (key == GRL_GSTREAMER_METADATA_KEY_DISCOVERY) {
      GValue value = { 0 };
      GstDiscovererInfo *info;

      g_object_get (resource, "info", &info, NULL);
      g_value_init (&value, GST_TYPE_DISCOVERER_INFO);
      g_value_set_object (&value, info);
      gst_discoverer_info_unref (info);
      grl_data_set (GRL_DATA (rs->media),
          GRL_GSTREAMER_METADATA_KEY_DISCOVERY, &value);
    }
  }
}

static void
resolve_from_resource (GrlSourceResolveSpec * rs, GomResource * resource)
{
  resolve_generic_discovery (rs, resource);
}

static void
discoverer_discovered_cb (GstDiscoverer * discoverer,
    GstDiscovererInfo * info, GError * err, GrlGStreamerSource * source)
{
  GomResource *resource = NULL;
  GList *tmp;
  GError *error = NULL;
  const gchar *uri = gst_discoverer_info_get_uri (info);
  GList *resolve_specs =
      g_hash_table_lookup (source->priv->resolve_specs_by_uri, uri);

  GRL_DEBUG ("%s %s was discovered", __func__, uri);

  if (!resolve_specs) {
    GRL_ERROR
        ("%s we have a discovery but no resolve specs for %s, it should not happen!",
        __func__, uri);
    return;
  }

  if (err) {
    GRL_WARNING ("%s failed to discover %s, %s", __func__, uri, err->message);
    error =
        g_error_new (GRL_CORE_ERROR, GRL_CORE_ERROR_RESOLVE_FAILED, "%s",
        err->message);
  } else if (gst_discoverer_info_get_result (info) != GST_DISCOVERER_OK) {
    GRL_WARNING ("%s failed to discover %s, discoverer result : %d", __func__,
        uri, gst_discoverer_info_get_result (info));
    error =
        g_error_new (GRL_CORE_ERROR, GRL_CORE_ERROR_RESOLVE_FAILED, "%s",
        "failed to discover uri");
  }

  if (!error) {
    GrlSourceResolveSpec *rs = (GrlSourceResolveSpec *) resolve_specs->data;
    resource = store_discovery (source, rs->media, info, &error);
  }

  for (tmp = resolve_specs; tmp; tmp = tmp->next) {
    GrlSourceResolveSpec *rs = (GrlSourceResolveSpec *) tmp->data;

    if (resource) {
      resolve_from_resource (rs, resource);
    }

    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data,
        error);
  }

  g_object_unref (resource);

  g_hash_table_remove (source->priv->resolve_specs_by_uri, uri);
}

static gboolean
keys_need_discovering (GList * keys)
{
  GList *tmp;
  gboolean need_discovering = FALSE;

  for (tmp = keys; tmp; tmp = tmp->next) {
    GrlKeyID key = GRLPOINTER_TO_KEYID (tmp->data);
    if (key == GRL_GSTREAMER_METADATA_KEY_DISCOVERY) {
      need_discovering = TRUE;
      break;
    }
  }

  return need_discovering;
}

static void
queue_uri_discovery (GrlGStreamerSource * gstsource, GrlSourceResolveSpec * rs)
{
  gboolean already_discovering_uri;
  const gchar *url = grl_media_get_url (rs->media);
  GList *resolve_specs =
      g_hash_table_lookup (gstsource->priv->resolve_specs_by_uri, url);

  already_discovering_uri = (resolve_specs != NULL);

  resolve_specs = g_list_append (resolve_specs, rs);
  g_hash_table_insert (gstsource->priv->resolve_specs_by_uri,
      (gpointer) g_strdup (url), resolve_specs);

  if (already_discovering_uri) {
    GRL_DEBUG ("%s %s is already being discovered", __func__, url);
    return;
  }

  GRL_DEBUG ("%s Starting discovery of %s", __func__, url);
  if (!gst_discoverer_discover_uri_async (gstsource->priv->discoverer, url)) {
    GError *error = g_error_new (GRL_CORE_ERROR, GRL_CORE_ERROR_RESOLVE_FAILED,
        "could not discover uri");

    GRL_WARNING ("%s discovery of %s failed", __func__, url);
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data,
        error);
  }
}

static void
grl_gstreamer_source_resolve (GrlSource * source, GrlSourceResolveSpec * rs)
{
  const gchar *url = grl_media_get_url (rs->media);
  GrlGStreamerSource *gstsource = GRL_GSTREAMER_SOURCE (source);
  GomResource *resource;

  if (!url) {
    GError *error = g_error_new (GRL_CORE_ERROR, GRL_CORE_ERROR_RESOLVE_FAILED,
        "could not discover NULL url");
    GRL_WARNING ("%s we can't discover a media with no url", __func__);
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data,
        error);
    return;
  }

  resource = find_resource (rs->media, gstsource->priv->repository);

  if (resource) {
    GRL_DEBUG ("%s the resource was already cached for %s", __func__, url);
    resolve_from_resource (rs, resource);
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, NULL);
    g_object_unref (resource);
  } else if (keys_need_discovering (rs->keys)) {
    queue_uri_discovery (gstsource, rs);
  } else {                      /* FIXME can that happen ? */
    GRL_WARNING ("%s we can't resolve anything for %s", __func__, url);
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, NULL);
  }
}

static const GList *
grl_gstreamer_source_supported_keys (GrlSource * source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_GSTREAMER_METADATA_KEY_DISCOVERY,
        NULL);
  }
  return keys;
}

static void
grl_gstreamer_source_set_property (GObject * object,
    guint propid, const GValue * value, GParamSpec * pspec)
{
  GrlGStreamerSourcePriv *priv = GRL_GSTREAMER_SOURCE_GET_PRIVATE (object);

  switch (propid) {
    case PROP_TIMEOUT:
      priv->timeout = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, propid, pspec);
  }
}

static void
grl_gstreamer_source_constructed (GObject * object)
{
  GrlGStreamerSource *source = GRL_GSTREAMER_SOURCE (object);

  source->priv->discoverer =
      gst_discoverer_new (source->priv->timeout * GST_SECOND, NULL);

  g_signal_connect (source->priv->discoverer, "discovered",
      G_CALLBACK (discoverer_discovered_cb), source);
  gst_discoverer_start (source->priv->discoverer);
}

static void
grl_gstreamer_source_finalize (GObject *object)
{
  GrlGStreamerSource *source = GRL_GSTREAMER_SOURCE (object);

  g_clear_object (&source->priv->repository);

  if (source->priv->adapter) {
    gom_adapter_close_sync (source->priv->adapter, NULL);
    g_clear_object (&source->priv->adapter);
  }

  g_hash_table_unref (source->priv->resolve_specs_by_uri);

  G_OBJECT_CLASS (grl_gstreamer_source_parent_class)->finalize (object);
}

static void
grl_gstreamer_source_class_init (GrlGStreamerSourceClass * klass)
{
  GrlSourceClass *source_class = GRL_SOURCE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = grl_gstreamer_source_set_property;

  g_object_class_install_property (gobject_class,
      PROP_TIMEOUT,
      g_param_spec_int ("discoverer-timeout",
          "discoverer-timeout",
          "The timeout in seconds to set on the GstDiscoverer",
          1, 3600, 1, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  source_class->may_resolve = grl_gstreamer_source_may_resolve;
  source_class->supported_keys = grl_gstreamer_source_supported_keys;
  source_class->resolve = grl_gstreamer_source_resolve;
  gobject_class->constructed = grl_gstreamer_source_constructed;
  gobject_class->finalize = grl_gstreamer_source_finalize;
  g_type_class_add_private (klass, sizeof (GrlGStreamerSourcePriv));
}

static void
migrate_cb (GObject * object, GAsyncResult * result, gpointer user_data)
{
  gboolean ret;
  GError *error = NULL;

  ret = gom_repository_migrate_finish (GOM_REPOSITORY (object), result, &error);
  if (!ret) {
    GRL_WARNING ("Failed to migrate database: %s", error->message);
    g_error_free (error);
  }
}

static void
grl_gstreamer_source_init (GrlGStreamerSource * source)
{
  GError *error = NULL;
  gchar *path;
  gchar *db_path;
  GList *object_types;

  source->priv = G_TYPE_INSTANCE_GET_PRIVATE (source,
      GRL_GSTREAMER_SOURCE_TYPE, GrlGStreamerSourcePriv);
  source->priv->resolve_specs_by_uri =
      g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  path = g_build_filename (g_get_user_data_dir (), "grilo-plugins", NULL);
  if (!g_file_test (path, G_FILE_TEST_IS_DIR)) {
    g_mkdir_with_parents (path, 0775);
  }

  GRL_DEBUG ("Opening database connection...");
  db_path = g_build_filename (path, GRL_SQL_DB, NULL);
  g_free (path);

  source->priv->adapter = gom_adapter_new ();
  if (!gom_adapter_open_sync (source->priv->adapter, db_path, &error)) {
    GRL_WARNING ("Could not open database '%s': %s", db_path, error->message);
    g_error_free (error);
    g_free (db_path);
    return;
  }
  g_free (db_path);

  source->priv->repository = gom_repository_new (source->priv->adapter);
  object_types =
      g_list_prepend (NULL, GINT_TO_POINTER (DISCOVERY_INFO_TYPE_RESOURCE));
  gom_repository_automatic_migrate_async (source->priv->repository, 2,
      object_types, migrate_cb, source);
}
