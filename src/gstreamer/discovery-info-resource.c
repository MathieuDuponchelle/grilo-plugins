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

#include <glib/gi18n.h>
#include <gst/pbutils/pbutils.h>

#include "discovery-info-resource.h"

G_DEFINE_TYPE (DiscoveryInfoResource, discovery_info_resource,
    GOM_TYPE_RESOURCE)

struct _DiscoveryInfoResourcePrivate
{
  GstDiscovererInfo *info;
  gchar *key;
};

enum
{
  PROP_0,
  PROP_KEY,
  PROP_INFO,
  LAST_PROP
};

static GParamSpec *specs[LAST_PROP];

static void discovery_info_resource_finalize (GObject * object)
{
  DiscoveryInfoResourcePrivate *priv = DISCOVERY_INFO_RESOURCE (object)->priv;

  g_free (priv->key);
  gst_discoverer_info_unref (priv->info);

  G_OBJECT_CLASS (discovery_info_resource_parent_class)->finalize (object);
}

static void
discovery_info_resource_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  DiscoveryInfoResource *resource = DISCOVERY_INFO_RESOURCE (object);

  switch (prop_id) {
    case PROP_KEY:
      g_value_set_string (value, resource->priv->key);
      break;
    case PROP_INFO:
      g_value_set_object (value, resource->priv->info);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
discovery_info_resource_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  DiscoveryInfoResource *resource = DISCOVERY_INFO_RESOURCE (object);

  switch (prop_id) {
    case PROP_KEY:
      g_free (resource->priv->key);
      resource->priv->key = g_value_dup_string (value);
      break;
    case PROP_INFO:
      resource->priv->info = gst_discoverer_info_ref(g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
info_from_bytes (GBytes * bytes, GValue * value)
{
  GVariant *variant =
      g_variant_new_from_bytes (G_VARIANT_TYPE_VARIANT, bytes, TRUE);
  GstDiscovererInfo *info = gst_discoverer_info_from_variant (variant);
  g_variant_unref (variant);
  g_value_init (value, GST_TYPE_DISCOVERER_INFO);
  g_value_take_object (value, info);
}

static GBytes *
info_to_bytes (GValue * value)
{
  GBytes *bytes;
  GstDiscovererInfo *info = g_value_get_object (value);
  GVariant *variant =
      gst_discoverer_info_to_variant (info, GST_DISCOVERER_SERIALIZE_BASIC);
  bytes = g_variant_get_data_as_bytes (variant);
  g_variant_unref (variant);

  return bytes;
}

static void
discovery_info_resource_class_init (DiscoveryInfoResourceClass * klass)
{
  GObjectClass *object_class;
  GomResourceClass *resource_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = discovery_info_resource_finalize;
  object_class->get_property = discovery_info_resource_get_property;
  object_class->set_property = discovery_info_resource_set_property;
  g_type_class_add_private (object_class,
      sizeof (DiscoveryInfoResourcePrivate));

  resource_class = GOM_RESOURCE_CLASS (klass);
  gom_resource_class_set_table (resource_class, "discovery_info");


  specs[PROP_KEY] =
      g_param_spec_string ("key", NULL, NULL, NULL, G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_KEY, specs[PROP_KEY]);

  specs[PROP_INFO] =
      g_param_spec_object ("info", "info", "A GStDiscovererInfo",
      GST_TYPE_DISCOVERER_INFO, G_PARAM_READWRITE);

  g_object_class_install_property (object_class, PROP_INFO, specs[PROP_INFO]);

  gom_resource_class_set_property_to_bytes (resource_class, "info",
      info_to_bytes, NULL);
  gom_resource_class_set_property_from_bytes (resource_class, "info",
      info_from_bytes, NULL);
  gom_resource_class_set_primary_key (resource_class, "key");
}

static void
discovery_info_resource_init (DiscoveryInfoResource * resource)
{
  resource->priv = G_TYPE_INSTANCE_GET_PRIVATE (resource,
      DISCOVERY_INFO_TYPE_RESOURCE, DiscoveryInfoResourcePrivate);
}
