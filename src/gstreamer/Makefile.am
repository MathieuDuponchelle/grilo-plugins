include $(top_srcdir)/gtester.mk

ext_LTLIBRARIES		 = libgrlgstreamer.la

libgrlgstreamer_la_CFLAGS =	\
	$(DEPS_GSTREAMER_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GrlGStreamer\" \
	-DLOCALEDIR=\"$(localedir)\"


libgrlgstreamer_la_LIBADD =	\
	$(DEPS_GSTREAMER_LIBS)

libgrllocalmetadata_la_LDFLAGS = \
	-no-undefined		 \
	-module			 \
	-avoid-version

libgrlgstreamer_la_SOURCES = grl-gstreamer.c grl-gstreamer.h discovery-info-resource.h discovery-info-resource.c

extdir						= $(GRL_PLUGINS_DIR)
gstreamerxmldir		= $(GRL_PLUGINS_DIR)
gstreamerxml_DATA	= $(GSTREAMER_PLUGIN_ID).xml

# This lets us test the plugin without installing it,
# because grilo expects the .so and .xml files to be in
# the same directory:
copy-xml-to-libs-dir: libgrlgstreamer.la
	cp -f $(srcdir)/$(gstreamerxml_DATA) $(builddir)/.libs/

all-local: copy-xml-to-libs-dir

EXTRA_DIST += $(gstreamerxml_DATA)

MAINTAINERCLEANFILES	 = \
	*.in \
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
