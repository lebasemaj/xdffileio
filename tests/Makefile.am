eol=

AM_CFLAGS = -I$(top_srcdir)/src \
	-DSRCDIR=\"$(abs_srcdir)\" \
	-DBUILDDIR=\"$(abs_builddir)\" \
	$(eol)

EXTRA_DIST = \
	ref128-13-97-50-11-7-1.bdf \
	ref128-13-97-50-11-7-1.edf \
	ref128-13-97-50-11-7-1.gdf1 \
	ref128-13-97-50-11-7-1.gdf2 \
	$(eol)

check_PROGRAMS = testbdf testedf testgdf1 testgdf2 readcheck gen-broken-gdf check-fixed-gdf
TESTS = testbdf testedf testgdf1 testgdf2 readcheck recompose-gdf.sh

if RUN_ERROR_TEST
check_PROGRAMS += errorcheck
TESTS += errorcheck
endif

testbdf_SOURCES = testbdf.c filecmp.c filecmp.h copy_xdf.c copy_xdf.h\
		  validation.c validation.h checkseek.h checkseek.c
testbdf_LDADD = $(top_builddir)/src/libxdffileio.la $(MMLIB_LIB)

testedf_SOURCES = testedf.c filecmp.c filecmp.h copy_xdf.c copy_xdf.h\
		  validation.c validation.h checkseek.h checkseek.c
testedf_LDADD = $(top_builddir)/src/libxdffileio.la $(MMLIB_LIB)

testgdf1_SOURCES = testgdf1.c filecmp.c filecmp.h copy_xdf.c copy_xdf.h\
		  validation.c validation.h checkseek.h checkseek.c
testgdf1_LDADD = $(top_builddir)/src/libxdffileio.la $(MMLIB_LIB)

testgdf2_SOURCES = testgdf2.c filecmp.c filecmp.h copy_xdf.c copy_xdf.h\
		  validation.c validation.h checkseek.h checkseek.c
testgdf2_LDADD = $(top_builddir)/src/libxdffileio.la $(MMLIB_LIB)

errorcheck_SOURCES = errorcheck.c filecmp.c filecmp.h
errorcheck_LDADD = $(top_builddir)/src/libxdffileio.la $(MMLIB_LIB)

readcheck_SOURCES = readcheck.c
readcheck_LDADD = $(top_builddir)/src/libxdffileio.la $(MMLIB_LIB)

gen_broken_gdf_SOURCES = gen-broken-gdf.c
gen_broken_gdf_LDFLAGS = -lm
gen_broken_gdf_LDADD = $(top_builddir)/src/libxdffileio.la $(MMLIB_LIB)

check_fixed_gdf_SOURCES = check-fixed-gdf.c
check_fixed_gdf_LDFLAGS = -lm
check_fixed_gdf_LDADD = $(top_builddir)/src/libxdffileio.la $(MMLIB_LIB)

# alias "test" and "check" targets targets
.PHONY: test
test: check

TEST_EXTENSIONS = .test .tap

AM_CPPFLAGS =
# if check support TAP output, make use of it
if TAP_SUPPORT_IN_CHECK
AM_CPPFLAGS += -DCHECK_SUPPORT_TAP
TAP_LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
	$(top_srcdir)/build-aux/tap-driver.sh
else
TAP_LOG_DRIVER = $(LOG_DRIVER)
endif  # TAP_SUPPORT_IN_CHECK

TESTS_ENVIRONMENT = EXECUTABLETEST=$(top_builddir)/tests EXECUTABLESRC=$(top_builddir)/src
TESTS += unittests.tap
check_PROGRAMS += unittests.tap

unittests_tap_SOURCES = \
	testcases.h \
	open_test.c \
	read_test.c \
	unittests.c \
	xdf_prepare_end_transfer_test.c \
	$(eol)

unittests_tap_CFLAGS = \
	$(AM_CFLAGS) \
	$(CHECK_CFLAGS) \
	$(eol)

unittests_tap_LDADD = \
	$(CHECK_LIBS) \
	$(top_builddir)/src/libxdffileio.la \
	$(MMLIB_LIB) \
	$(eol)
