include $(top_srcdir)/gtester.mk

TEST_PROGS +=	\
   test_gstreamer

test_gstreamer_defines =                                              \
    -DGSTREAMER_PLUGIN_PATH=\""$(abs_top_builddir)/src/gstreamer/.libs/"\" \
    -DFILESYSTEM_PLUGIN_PATH=\""$(abs_top_builddir)/src/filesystem/.libs/"\" \
    -DGSTREAMER_DATA_PATH=\""$(abs_top_srcdir)/tests/gstreamer/data/"\"

test_gstreamer_SOURCES =	\
	test_gstreamer.c

test_gstreamer_LDADD =	\
	@DEPS_LIBS@ \
	$(DEPS_GSTREAMER_LIBS)


test_gstreamer_CFLAGS =	\
	@DEPS_CFLAGS@		\
	$(DEPS_GSTREAMER_CFLAGS) \
	$(test_gstreamer_defines)

# Distribute the tests data:
dist_noinst_DATA =         \
   data/theora-vorbis.ogg

noinst_PROGRAMS = $(TEST_PROGS)

MAINTAINERCLEANFILES =  \
   *.in                 \
   *~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
