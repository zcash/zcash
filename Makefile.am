#
# Makefile.am: Top-level Automake input for Zcash/Bitcoin Core fork projects.
# Manages compilation, testing, and distribution tasks.
#

# Pattern rule for debugging Automake variables: e.g., 'make print-top_srcdir'
print-%:
	@echo $* = $($*)

# Standard flags for Automake build configuration.
ACLOCAL_AMFLAGS = -I build-aux/m4
SUBDIRS = src
if ENABLE_MAN
SUBDIRS += doc/man
endif
.PHONY: deploy FORCE rpc-tests

# Use ZCASH_LIBS for conditional compilation of Zcash-specific libraries (was BITCOIN_LIBS)
if BUILD_ZCASH_LIBS
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libzcash_script.pc
endif

# Binary file locations
BITCOIND_BIN=$(top_builddir)/src/$(BITCOIN_DAEMON_NAME)$(EXEEXT)
BITCOIN_CLI_BIN=$(top_builddir)/src/$(BITCOIN_CLI_NAME)$(EXEEXT)

# Documentation files to include in the distribution tarball
DIST_DOCS = $(wildcard doc/*.md) $(wildcard doc/release-notes/*.md)

# Security and Symbol check scripts
BIN_CHECKS=$(top_srcdir)/contrib/devtools/symbol-check.py \
           $(top_srcdir)/contrib/devtools/security-check.py

# List of all intermediate coverage info files
COVERAGE_INFO = baseline_filtered_combined.info baseline.info \
  leveldb_baseline.info test_bitcoin_filtered.info total_coverage.info \
  baseline_filtered.info \
  leveldb_baseline_filtered.info test_bitcoin_coverage.info test_bitcoin.info \
  zcash-gtest.info zcash-gtest_filtered.info zcash-gtest_coverage.info

# Hook to include dynamically generated files in the distribution tarball
dist-hook:
	-$(GIT) archive --format=tar HEAD -- src/clientversion.cpp | $(AMTAR) -C $(top_distdir) -xf -

# Pass build targets to the src directory Makefile
$(if $(findstring src/,$(MAKECMDGOALS)),$(MAKECMDGOALS), none): FORCE
	$(MAKE) -C src $(patsubst src/%,%,$@)

# Rules to build specific binaries
$(BITCOIND_BIN): FORCE
	$(MAKE) -C src $(@F)

$(BITCOIN_CLI_BIN): FORCE
	$(MAKE) -C src $(@F)

# Security and symbol check targets
check-security: FORCE
	$(MAKE) -C src check-security

check-symbols: FORCE
	$(MAKE) -C src check-symbols

# --- Code Coverage (LCOV) Section ---
if USE_LCOV

# Common filters to exclude system headers, dependency headers, and test files from coverage reports
LCOV_FILTERS = "/usr/include/*" \
              "$(abs_builddir)/depends/x86_64-pc-linux-gnu/include/*.h" \
              "$(abs_builddir)/depends/x86_64-pc-linux-gnu/include/boost/*" \
              "$(abs_builddir)/depends/x86_64-pc-linux-gnu/include/gmock/*" \
              "$(abs_builddir)/depends/x86_64-pc-linux-gnu/include/gtest/*" \
              "$(abs_builddir)/src/gtest/*" \
              "$(abs_builddir)/src/test/*" \
              "$(abs_builddir)/src/wallet/gtest/*" \
              "$(abs_builddir)/src/wallet/test/*"

baseline.info:
	$(LCOV) -c -i -d $(abs_builddir)/src -o $@

baseline_filtered.info: baseline.info
	$(LCOV) -r $< $(LCOV_FILTERS) -o $@

leveldb_baseline.info: baseline_filtered.info
	$(LCOV) -c -i -d $(abs_builddir)/src/leveldb -b $(abs_builddir)/src/leveldb -o $@

leveldb_baseline_filtered.info: leveldb_baseline.info
	$(LCOV) -r $< $(LCOV_FILTERS) -o $@

baseline_filtered_combined.info: leveldb_baseline_filtered.info baseline_filtered.info
	$(LCOV) -a leveldb_baseline_filtered.info -a baseline_filtered.info -o $@

test_bitcoin.info: baseline_filtered_combined.info
	$(MAKE) -C src/ bitcoin_test_check
	$(LCOV) -c -d $(abs_builddir)/src -t test_bitcoin -o $@
	$(LCOV) -z -d $(abs_builddir)/src
	$(LCOV) -z -d $(abs_builddir)/src/leveldb

test_bitcoin_filtered.info: test_bitcoin.info
	$(LCOV) -r $< $(LCOV_FILTERS) -o $@

z
