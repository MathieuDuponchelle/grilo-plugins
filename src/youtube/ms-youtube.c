/*
 * Copyright (C) 2010 Igalia S.L.
 *
 * Contact: Iago Toral Quiroga <itoral@igalia.com>
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

#include <media-store.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <string.h>
#include <stdlib.h>

#include "ms-youtube.h"

#define YOUTUBE_MOST_VIEWED_URL  "http://gdata.youtube.com/feeds/standardfeeds/most_viewed?start-index=%d&max-results=%d"
#define YOUTUBE_VIDEO_INFO_URL   "http://www.youtube.com/get_video_info?video_id=%s"
#define YOUTUBE_VIDEO_URL        "http://www.youtube.com/get_video?video_id=%s&t=%s"
#define YOUTUBE_SEARCH_URL       "http://gdata.youtube.com/feeds/api/videos?vq=%s&start-index=%d&max-results=%d"

#define PLUGIN_ID   "ms-youtube"
#define PLUGIN_NAME "Youtube"
#define PLUGIN_DESC "A plugin for browsing and searching Youtube videos"

#define SOURCE_ID   "ms-youtube"
#define SOURCE_NAME "Youtube"
#define SOURCE_DESC "A source for browsing and searching Youtube videos"

#define AUTHOR      "Igalia S.L."
#define LICENSE     "LGPL"
#define SITE        "http://www.igalia.com"

typedef struct {
  gchar *id;
  gchar *title;
  gchar *published;
  gchar *author;
  gchar *thumbnail;
  gchar *description;
  gchar *duration;
  gboolean restricted;
} Entry; 

typedef struct {
  MsMediaSource *source;
  guint operation_id;
  const GList *keys;
  guint skip;
  guint count;
  MsMediaSourceResultCb callback;
  gpointer user_data;
} OperationSpec;

typedef struct {
  MsMetadataSource *source;
  const GList *keys;
  MsMetadataSourceResultCb callback;
  gpointer user_data;
} MetadataOperationSpec;

static MsYoutubeSource *ms_youtube_source_new (void);

gboolean ms_youtube_plugin_init (MsPluginRegistry *registry,
				 const MsPluginInfo *plugin);

static const GList *ms_youtube_source_supported_keys (MsMetadataSource *source);

static void ms_youtube_source_metadata (MsMetadataSource *source,
					const gchar *object_id,
					const GList *keys,
					MsMetadataSourceResultCb callback,
					gpointer user_data);

static void ms_youtube_source_search (MsMediaSource *source,
				      guint browse_id,
				      const gchar *text,
				      const GList *keys,
				      const gchar *filter,
				      guint skip,
				      guint count,
				      MsMediaSourceResultCb callback,
				      gpointer user_data);

static void ms_youtube_source_browse (MsMediaSource *source, 
				      guint search_id,
				      const gchar *container_id,
				      const GList *keys,
				      guint skip,
				      guint count,
				      MsMediaSourceResultCb callback,
				      gpointer user_data);

static gchar *read_url (const gchar *url);

/* =================== Youtube Plugin  =============== */

gboolean
ms_youtube_plugin_init (MsPluginRegistry *registry, const MsPluginInfo *plugin)
{
  g_debug ("youtube_plugin_init\n");
  MsYoutubeSource *source = ms_youtube_source_new ();
  ms_plugin_registry_register_source (registry, plugin, MS_MEDIA_PLUGIN (source));
  return TRUE;
}

MS_PLUGIN_REGISTER (ms_youtube_plugin_init, 
                    NULL, 
                    PLUGIN_ID,
                    PLUGIN_NAME, 
                    PLUGIN_DESC, 
                    PACKAGE_VERSION,
                    AUTHOR, 
                    LICENSE, 
                    SITE);

/* ================== Youtube GObject ================ */

static MsYoutubeSource *
ms_youtube_source_new (void)
{
  g_debug ("ms_youtube_source_new");
  return g_object_new (MS_YOUTUBE_SOURCE_TYPE,
		       "source-id", SOURCE_ID,
		       "source-name", SOURCE_NAME,
		       "source-desc", SOURCE_DESC,
		       NULL);
}

static void
ms_youtube_source_class_init (MsYoutubeSourceClass * klass)
{
  MsMediaSourceClass *source_class = MS_MEDIA_SOURCE_CLASS (klass);
  MsMetadataSourceClass *metadata_class = MS_METADATA_SOURCE_CLASS (klass);
  source_class->browse = ms_youtube_source_browse;
  source_class->search = ms_youtube_source_search;
  metadata_class->metadata = ms_youtube_source_metadata;
  metadata_class->supported_keys = ms_youtube_source_supported_keys;

  if (!gnome_vfs_init ()) {
    g_error ("Failed to initialize Gnome VFS");
  }
}

static void
ms_youtube_source_init (MsYoutubeSource *source)
{
}

G_DEFINE_TYPE (MsYoutubeSource, ms_youtube_source, MS_TYPE_MEDIA_SOURCE);

/* ======================= Utilities ==================== */

static void
print_entry (Entry *entry)
{
  g_print ("Entry Information:\n");
  g_print ("            ID: %s (%d)\n", entry->id, entry->restricted);
  g_print ("         Title: %s\n", entry->title);
  g_print ("          Date: %s\n", entry->published);
  g_print ("        Author: %s\n", entry->author);
  g_print ("      Duration: %s\n", entry->duration);
  g_print ("     Thumbnail: %s\n", entry->thumbnail);
  g_print ("   Description: %s\n", entry->description);
}

static void
free_entry (Entry *entry)
{
  g_free (entry->id);
  g_free (entry->title);
  g_free (entry->published);
  g_free (entry->author);
  g_free (entry->duration);
  g_free (entry->thumbnail);
  g_free (entry->description);
  g_free (entry);
}

static gchar *
get_video_url (const gchar *id)
{
  gchar *token_start;
  gchar *token_end;
  gchar *token;
  gchar *video_info_url;
  gchar *data;
  gchar *url;

  video_info_url = g_strdup_printf (YOUTUBE_VIDEO_INFO_URL, id);
  data = read_url (video_info_url);
  if (!data) {
    g_free (video_info_url);
    return NULL;
  }

  token_start = g_strrstr (data, "&token=");
  if (!token_start) {
    g_free (video_info_url);
    return NULL;
  }
  token_start += 7;
  token_end = strstr (token_start, "&");
  token = g_strndup (token_start, token_end - token_start);

  url = g_strdup_printf (YOUTUBE_VIDEO_URL, id, token);

  g_free (video_info_url);
  g_free (token);
  
  return url;
}

static gchar *
read_url (const gchar *url)
{
  gchar buffer[1025];
  GnomeVFSFileSize bytes_read;
  GnomeVFSResult r;
  GString *data;
  GnomeVFSHandle *fh;

  /* Open URL */
  g_debug ("Opening '%s'", url);
  r = gnome_vfs_open (&fh, url, GNOME_VFS_OPEN_READ);
  if (r != GNOME_VFS_OK) {
    g_warning ("Failed to open '%s'", url);
    return NULL;
  }

  /* Read URL contents */
  g_debug ("Reading data from '%s'", url);
  data = g_string_new ("");
  do {
    gnome_vfs_read (fh, buffer, 1024, &bytes_read);
    buffer[bytes_read] = '\0';
    g_string_append (data, buffer);
  } while (bytes_read > 0);
  g_debug ("  Done reading data from url");

  gnome_vfs_close (fh);

  return g_string_free (data, FALSE);
}

static MsContent *
build_media_from_entry (const Entry *entry, const GList *keys)
{
  MsContentMedia *media;
  gchar *url;
  GList *iter;

  media = ms_content_media_new (); 

  iter = (GList *) keys;
  while (iter) {
    MsKeyID key_id = GPOINTER_TO_UINT (iter->data);
    switch (key_id) {
    case MS_METADATA_KEY_ID:
      ms_content_media_set_id (media, entry->id);
      break;
    case MS_METADATA_KEY_TITLE:
      ms_content_media_set_title (media, entry->title);
      break;
    case MS_METADATA_KEY_AUTHOR:
      ms_content_media_set_author (media, entry->author);
      break;
    case MS_METADATA_KEY_DESCRIPTION:
      ms_content_media_set_description (media, entry->description);
      break;
    case MS_METADATA_KEY_THUMBNAIL:
      ms_content_media_set_thumbnail (media, entry->thumbnail);
      break;
    case MS_METADATA_KEY_DATE:
      ms_content_set_string (MS_CONTENT (media), 
			     MS_METADATA_KEY_DATE, entry->published);
      break;
    case MS_METADATA_KEY_DURATION:
      ms_content_set_string (MS_CONTENT (media), 
			     MS_METADATA_KEY_DURATION, entry->duration);
      break;
    case MS_METADATA_KEY_URL:
      if (!entry->restricted) {
	url = get_video_url (entry->id);
	if (url) {
	  ms_content_media_set_url (media, url);
	}
	g_free (url);
      }
      break;
    default:
      break;
    }
    iter = g_list_next (iter);
  }

  return MS_CONTENT (media);
}

static gchar *
parse_id (xmlDocPtr doc, xmlNodePtr id)
{
  gchar *value, *video_id;
  value = (gchar *) xmlNodeListGetString (doc, id->xmlChildrenNode, 1);
  video_id = g_strdup (g_strrstr (value , "/") + 1);
  g_free (value);
  return video_id;
}

static gchar *
parse_author (xmlDocPtr doc, xmlNodePtr author)
{
  xmlNodePtr node;
  node = author->xmlChildrenNode;
  while (node) {
    if (!xmlStrcmp (node->name, (const xmlChar *) "name")) {
      return (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    }
    node = node->next;
  }
  return NULL;
}

static void
parse_media_group (xmlDocPtr doc, xmlNodePtr media, Entry *entry)
{
  xmlNodePtr node;
  xmlNs *ns;

  node = media->xmlChildrenNode;
  while (node) {
    ns = node->ns;
    if (!xmlStrcmp (node->name, (const xmlChar *) "description")) {
      entry->description =
	(gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    } else if (!xmlStrcmp (node->name, (const xmlChar *) "thumbnail") &&
	       !entry->thumbnail) {
      entry->thumbnail = 
	(gchar *) xmlGetProp (node, (xmlChar *) "url");
    } else if (!xmlStrcmp (node->name, (const xmlChar *) "duration")) {
      entry->duration =
	(gchar *) xmlGetProp (node, (xmlChar *) "seconds");
    }
    node = node->next;
  }    
}

static void
parse_app_control (xmlDocPtr doc, xmlNodePtr media, Entry *entry)
{
  xmlNodePtr node;
  xmlChar *value;

  node = media->xmlChildrenNode;
  while (node) {
    if (!xmlStrcmp (node->name, (const xmlChar *) "state")) {
      value = xmlGetProp (node, (xmlChar *) "name");
      if (!xmlStrcmp (value, (xmlChar *) "restricted")) {
	entry->restricted = TRUE;
      }
      g_free ((gchar *) value);
    }
    node = node->next;
  }    
}

static void
parse_entry (xmlDocPtr doc, xmlNodePtr entry, Entry *data)
{
  xmlNodePtr node;
  xmlNs *ns;

  node = entry->xmlChildrenNode;
  while (node) {
    ns = node->ns;
    if (!xmlStrcmp (node->name, (const xmlChar *) "id")) {
      data->id = parse_id (doc, node);
    } else if (!xmlStrcmp (node->name, (const xmlChar *) "published")) {
      data->published = 
	(gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    } else if (!xmlStrcmp (node->name, (const xmlChar *) "title")) {
      data->title = 
	(gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    } else if (!xmlStrcmp (node->name, (const xmlChar *) "author")) {
      data->author = parse_author (doc, node);
    } else if (ns && !xmlStrcmp (ns->prefix, (const xmlChar *) "media") && 
	       !xmlStrcmp (node->name, (const xmlChar *) "group")) {
      parse_media_group (doc, node, data);
    } else if (ns && !xmlStrcmp (ns->prefix, (const xmlChar *) "app") && 
	       !xmlStrcmp (node->name, (const xmlChar *) "control")) {
      parse_app_control (doc, node, data);
    }

    node = node->next;
  }
}

static void
parse_entries (OperationSpec *os, xmlDocPtr doc, xmlNodePtr node)
{
  guint index = 0, total_results = 0;
  xmlNs *ns;

  /* First checkout search information looking for totalResults */
  while (node && !total_results) {
    if (!xmlStrcmp (node->name, (const xmlChar *) "entry")) {
      break;
    } else {
      ns = node->ns;
      if (ns && !xmlStrcmp (ns->prefix, (xmlChar *) "openSearch")) {
	if (!xmlStrcmp (node->name, (const xmlChar *) "totalResults")) {
	  gchar *total_results_str = 
	    (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
	  total_results = atoi (total_results_str);
	  g_free (total_results_str);
	}
      }
    }
    node = node->next;
  }

  /* Compute # of items to send */
  if (total_results >= os->skip + os->count) {
    total_results = os->count;
  } else if (total_results > os->skip) {
    total_results -= os->skip;
  } else {
    total_results = 0;
  }

  /* Now go for the entries */
  while (node) {
    while (node && xmlStrcmp (node->name, (const xmlChar *) "entry")) {
      node = node->next;
    }
    if (node) {
      Entry *entry = g_new0 (Entry, 1);
      parse_entry (doc, node, entry);
      if (0) print_entry (entry);
      MsContent *media = build_media_from_entry (entry, os->keys);
      free_entry (entry);

      index++;
      os->callback (os->source,
		    os->operation_id,
		    media,
		    total_results - index,
		    os->user_data,
		    NULL);

      node = node->next;
    }
  }
}

static void
parse_feed (OperationSpec *os, const gchar *str, GError **error)
{
  xmlDocPtr doc;
  xmlNodePtr node;
  
  doc = xmlRecoverDoc ((xmlChar *) str);
  if (!doc) {
    *error = g_error_new (MS_ERROR, 
			  MS_ERROR_BROWSE_FAILED,
			  "Failed to parse Youtube's response");
    goto free_resources;
  }

  node = xmlDocGetRootElement (doc);
  if (!node) {
    *error = g_error_new (MS_ERROR, 
			  MS_ERROR_BROWSE_FAILED,
			  "Empty response from Youtube");
    goto free_resources;
  }

  if (xmlStrcmp (node->name, (const xmlChar *) "feed")) {
    *error = g_error_new (MS_ERROR, 
			  MS_ERROR_BROWSE_FAILED,
			  "Unexpected response from Youtube: no feed");
    goto free_resources;
  }

  node = node->xmlChildrenNode;
  if (!node) {
    *error = g_error_new (MS_ERROR, 
			  MS_ERROR_BROWSE_FAILED,
			  "Unexpected response from Youtube: empty feed");
    goto free_resources;
  }

  parse_entries (os, doc, node);
  
 free_resources:
  xmlFreeDoc (doc);
  return;
}

static MsContent *
parse_metadata_entry (MetadataOperationSpec *os,
		      xmlDocPtr doc,
		      xmlNodePtr node,
		      GError **error)
{
  xmlNs *ns;
  guint total_results = 0;
  MsContent *media = NULL;

  /* First checkout search information looking for totalResults */
  while (node && !total_results) {
    if (!xmlStrcmp (node->name, (const xmlChar *) "entry")) {
      break;
    } else {
      ns = node->ns;
      if (ns && !xmlStrcmp (ns->prefix, (xmlChar *) "openSearch")) {
	if (!xmlStrcmp (node->name, (const xmlChar *) "totalResults")) {
	  gchar *total_results_str = 
	    (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
	  total_results = atoi (total_results_str);
	  g_free (total_results_str);
	}
      }
    }
    node = node->next;
  }

  /* Should have exactly 1 result */
  if (total_results != 1) {
    *error = g_error_new (MS_ERROR, 
			  MS_ERROR_MEDIA_NOT_FOUND,
			  "Could not find requested media");
    return NULL;
  }

  /* Now go for the entry data */
  while (node && xmlStrcmp (node->name, (const xmlChar *) "entry")) {
    node = node->next;
  }
  if (node) {
    Entry *entry = g_new0 (Entry, 1);
    parse_entry (doc, node, entry);    
    if (0) print_entry (entry);
    media = build_media_from_entry (entry, os->keys);    
    free_entry (entry);
  } 

  return media;
}

static MsContent *
parse_metadata_feed (MetadataOperationSpec *os,
		     const gchar *str,
		     GError **error)
{
  xmlDocPtr doc;
  xmlNodePtr node;
  MsContent *media = NULL;
  
  doc = xmlRecoverDoc ((xmlChar *) str);
  if (!doc) {
    *error = g_error_new (MS_ERROR, 
			  MS_ERROR_METADATA_FAILED,
			  "Failed to parse Youtube's response");
    goto free_resources;
  }

  node = xmlDocGetRootElement (doc);
  if (!node) {
    *error = g_error_new (MS_ERROR, 
			  MS_ERROR_METADATA_FAILED,
			  "Empty response from Youtube");
    goto free_resources;
  }

  if (xmlStrcmp (node->name, (const xmlChar *) "feed")) {
    *error = g_error_new (MS_ERROR, 
			  MS_ERROR_METADATA_FAILED,
			  "Unexpected response from Youtube: no feed");
    goto free_resources;
  }

  node = node->xmlChildrenNode;
  if (!node) {
    *error = g_error_new (MS_ERROR, 
			  MS_ERROR_METADATA_FAILED,
			  "Unexpected response from Youtube: empty feed");
    goto free_resources;
  }

  media = parse_metadata_entry (os, doc, node, error);
  
 free_resources:
  xmlFreeDoc (doc);
  return media;
}

/* ================== API Implementation ================ */

static const GList *
ms_youtube_source_supported_keys (MsMetadataSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = ms_metadata_key_list_new (MS_METADATA_KEY_TITLE, 
				     MS_METADATA_KEY_URL,
				     MS_METADATA_KEY_AUTHOR,
				     MS_METADATA_KEY_DESCRIPTION,
				     MS_METADATA_KEY_DURATION,
				     MS_METADATA_KEY_DATE,
				     MS_METADATA_KEY_THUMBNAIL,
				     NULL);
  }
  return keys;
}

static void
ms_youtube_source_browse (MsMediaSource *source, 
			  guint browse_id,
			  const gchar *container_id,
			  const GList *keys,
			  guint skip,
			  guint count,
			  MsMediaSourceResultCb callback,
			  gpointer user_data)
{
  gchar *xmldata, *url;
  OperationSpec *os;
  GError *error = NULL;

  g_debug ("ms_youtube_source_browse");

  /* TODO: create root categories */

  url = g_strdup_printf (YOUTUBE_MOST_VIEWED_URL, skip + 1, count);
  xmldata = read_url (url);
  g_free (url);

  if (!xmldata) {
    GError *error = g_error_new (MS_ERROR,
				 MS_ERROR_BROWSE_FAILED,
				 "Failed to connect to Youtube");
    callback (source, browse_id, NULL, 0, user_data, error);    
    g_error_free (error);
    return;
  }

  /* TODO: ref/copy stuff in the base class */
  os = g_new0 (OperationSpec, 1);
  os->source = g_object_ref (source);
  os->operation_id = browse_id;
  os->keys = g_list_copy ((GList *) keys);
  os->skip = skip;
  os->count = count;
  os->callback = callback;
  os->user_data = user_data;

  parse_feed (os, xmldata, &error);
  g_free (xmldata);

  if (error) {
    os->callback (os->source, os->operation_id, NULL, 0, os->user_data, error);
    g_error_free (error);
  }
}

static void
ms_youtube_source_search (MsMediaSource *source,
			  guint search_id,
			  const gchar *text,
			  const GList *keys,
			  const gchar *filter,
			  guint skip,
			  guint count,
			  MsMediaSourceResultCb callback,
			  gpointer user_data)
{
  gchar *xmldata, *url;
  OperationSpec *os;
  GError *error = NULL;

  g_debug ("ms_youtube_source_search");

  if (filter) {
    g_warning ("Search filter not supported, ignoring filter argument");
  }

  url = g_strdup_printf (YOUTUBE_SEARCH_URL, text, skip + 1, count);
  xmldata = read_url (url);
  g_free (url);

  if (!xmldata) {
    GError *error = g_error_new (MS_ERROR,
				 MS_ERROR_SEARCH_FAILED,
				 "Failed to connect to Youtube");
    callback (source, search_id, NULL, 0, user_data, error);    
    g_error_free (error);
    return;
  }

  os = g_new0 (OperationSpec, 1);
  os->source = g_object_ref (source);
  os->operation_id = search_id;
  os->keys = g_list_copy ((GList *) keys);
  os->skip = skip;
  os->count = count;
  os->callback = callback;
  os->user_data = user_data;

  parse_feed (os, xmldata, &error);
  g_free (xmldata);

  if (error) {
    error->code = MS_ERROR_SEARCH_FAILED;
    os->callback (os->source, os->operation_id, NULL, 0, os->user_data, error);
    g_error_free (error);
  }
}

static void
ms_youtube_source_metadata (MsMetadataSource *source,
			    const gchar *object_id,
			    const GList *keys,
			    MsMetadataSourceResultCb callback,
			    gpointer user_data)
{
  gchar *xmldata, *url;
  MetadataOperationSpec *os;
  GError *error = NULL;
  MsContent *media;

  g_debug ("ms_youtube_source_metadata");

  /* For metadata retrieval we just search by text using the video ID */
  url = g_strdup_printf (YOUTUBE_SEARCH_URL, object_id, 1, 1);
  xmldata = read_url (url);
  g_free (url);

  if (!xmldata) {
    GError *error = g_error_new (MS_ERROR,
				 MS_ERROR_METADATA_FAILED,
				 "Failed to connect to Youtube");
    callback (source, NULL, user_data, error);    
    g_error_free (error);
    return;
  }

  os = g_new0 (MetadataOperationSpec, 1);
  os->source = g_object_ref (source);
  os->keys = g_list_copy ((GList *) keys);
  os->callback = callback;
  os->user_data = user_data;

  media = parse_metadata_feed (os, xmldata, &error);
  g_free (xmldata);

  if (error) {
    os->callback (os->source, NULL, os->user_data, error);
    g_error_free (error);
  } else {
    os->callback (os->source, media, os->user_data, NULL);
  }
}