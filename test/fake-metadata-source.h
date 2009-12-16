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

#ifndef _FAKE_METADATA_SOURCE_H_
#define _FAKE_METADATA_SOURCE_H_

#include "../src/metadata-source.h"

#define FAKE_METADATA_SOURCE_TYPE   (fake_metadata_source_get_type ())
#define FAKE_METADATA_SOURCE(obj)  (G_TYPE_CHECK_INSTANCE_CAST ((obj), FAKE_METADATA_SOURCE_TYPE, Fakemetadatasource))
#define IS_FAKE_METADATA_SOURCE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FAKE_METADATA_SOURCE_TYPE))
#define FAKE_METADATA_SOURCE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), FAKE_METADATA_SOURCE_TYPE,  FakeMetadataSourceClass))
#define IS_FAKE_METADATA_SOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), FAKE_METADATA_SOURCE_TYPE))
#define FAKE_METADATA_SOURCE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), FAKE_METADATA_SOURCE_TYPE, FakeMetadataSourceClass))

typedef struct _FakeMetadataSource FakeMetadataSource;

struct _FakeMetadataSource {

  MetadataSource parent;

};

typedef struct _FakeMetadataSourceClass FakeMetadataSourceClass;

struct _FakeMetadataSourceClass {

  MetadataSourceClass parent_class;

};

GType fake_metadata_source_get_type (void);

FakeMetadataSource *fake_metadata_source_new (void);

#endif