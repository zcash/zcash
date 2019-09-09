# Copyright (c) 2016-2019 The Zcash developers
# Copyright (c) 2013-2019 The Bitcoin Core developers
# Copyright (c) 2013-2019 Bitcoin Developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

ACLOCAL_AMFLAGS = -I build-aux/m4
SUBDIRS = src
if ENABLE_MAN
SUBDIRS += doc/man
endif
.PHONY: deploy FORCE

GZIP_ENV="-9n"

if BUILD_BITCOIN_LIBS
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libzcashconsensus.pc
endif

BITCOIND_BIN=$(top_builddir)/src/$(BITCOIN_DAEMON_NAME)$(EXEEXT)
BITCOIN_CLI_BIN=$(top_builddir)/src/$(BITCOIN_CLI_NAME)$(EXEEXT)

DIST_DOCS = $(wildcard doc/*.md) $(wildcard doc/release-notes/*.md)

BIN_CHECKS=$(top_srcdir)/contrib/devtools/symbol-check.py \
           $(top_srcdir)/contrib/devtools/security-check.py

COVERAGE_INFO = baseline_filtered_combined.info baseline.info \
  leveldb_baseline.info test_bitcoin_filtered.info total_coverage.info \
  baseline_filtered.info \
  leveldb_baseline_filtered.info test_bitcoin_coverage.info test_bitcoin.info \
  zcash-gtest.info zcash-gtest_filtered.info zcash-gtest_coverage.info

dist-hook:
	-$(MAKE) -C $(top_distdir)/src/leveldb clean
	-$(MAKE) -C $(top_distdir)/src/secp256k1 distclean
	-$(GIT) archive --format=tar HEAD -- src/clientversion.cpp | $(AMTAR) -C $(top_distdir) -xf -

distcheck-hook:
	$(MKDIR_P) $(top_distdir)/_build/src/leveldb
	cp -rf $(top_srcdir)/src/leveldb/* $(top_distdir)/_build/src/leveldb/
	-$(MAKE) -C $(top_distdir)/_build/src/leveldb clean

distcleancheck:
	@:

$(if $(findstring src/,$(MAKECMDGOALS)),$(MAKECMDGOALS), none): FORCE
	$(MAKE) -C src $(patsubst src/%,%,$@)

$(BITCOIND_BIN): FORCE
	$(MAKE) -C src $(@F)

$(BITCOIN_CLI_BIN): FORCE
	$(MAKE) -C src $(@F)

if USE_LCOV

baseline.info:
	$(LCOV) -c -i -d $(abs_builddir)/src -o $@

baseline_filtered.info: baseline.info
	$(LCOV) -r $< "/usr/include/*" \
                      "$(abs_builddir)/depends/x86_64-unknown-linux-gnu/include/*.h" \
                      "$(abs_builddir)/depends/x86_64-unknown-linux-gnu/include/boost/*" \
                      "$(abs_builddir)/depends/x86_64-unknown-linux-gnu/include/gmock/*" \
                      "$(abs_builddir)/depends/x86_64-unknown-linux-gnu/include/gtest/*" \
                      "$(abs_builddir)/src/gtest/*" \
                      "$(abs_builddir)/src/test/*" \
                      "$(abs_builddir)/src/wallet/gtest/*" \
                      "$(abs_builddir)/src/wallet/test/*" \
                      -o $@

leveldb_baseline.info: baseline_filtered.info
	$(LCOV) -c -i -d $(abs_builddir)/src/leveldb -b $(abs_builddir)/src/leveldb -o $@

leveldb_baseline_filtered.info: leveldb_baseline.info
	$(LCOV) -r $< "/usr/include/*" \
                      "$(abs_builddir)/depends/x86_64-unknown-linux-gnu/include/*.h" \
                      "$(abs_builddir)/depends/x86_64-unknown-linux-gnu/include/boost/*" \
                      "$(abs_builddir)/depends/x86_64-unknown-linux-gnu/include/gmock/*" \
                      "$(abs_builddir)/depends/x86_64-unknown-linux-gnu/include/gtest/*" \
                      "$(abs_builddir)/src/gtest/*" \
                      "$(abs_builddir)/src/test/*" \
                      "$(abs_builddir)/src/wallet/gtest/*" \
                      "$(abs_builddir)/src/wallet/test/*" \
                      -o $@

baseline_filtered_combined.info: leveldb_baseline_filtered.info baseline_filtered.info
	$(LCOV) -a leveldb_baseline_filtered.info -a baseline_filtered.info -o $@

test_bitcoin.info: baseline_filtered_combined.info
	$(MAKE) -C src/ bitcoin_test_check
	$(LCOV) -c -d $(abs_builddir)/src -t test_bitcoin -o $@
	$(LCOV) -z -d $(abs_builddir)/src
	$(LCOV) -z -d $(abs_builddir)/src/leveldb

test_bitcoin_filtered.info: test_bitcoin.info
	$(LCOV) -r $< "/usr/include/*" \
                      "$(abs_builddir)/depends/x86_64-unknown-linux-gnu/include/*.h" \
                      "$(abs_builddir)/depends/x86_64-unknown-linux-gnu/include/boost/*" \
                      "$(abs_builddir)/depends/x86_64-unknown-linux-gnu/include/gmock/*" \
                      "$(abs_builddir)/depends/x86_64-unknown-linux-gnu/include/gtest/*" \
                      "$(abs_builddir)/src/gtest/*" \
                      "$(abs_builddir)/src/test/*" \
                      "$(abs_builddir)/src/wallet/gtest/*" \
                      "$(abs_builddir)/src/wallet/test/*" \
                      -o $@

zcash-gtest.info: baseline_filtered_combined.info
	$(MAKE) -C src/ zcash-gtest_check
	$(LCOV) -c -d $(abs_builddir)/src -t zcash-gtest -o $@
	$(LCOV) -z -d $(abs_builddir)/src
	$(LCOV) -z -d $(abs_builddir)/src/leveldb

zcash-gtest_filtered.info: zcash-gtest.info
	$(LCOV) -r $< "/usr/include/*" \
                      "$(abs_builddir)/depends/x86_64-unknown-linux-gnu/include/*.h" \
                      "$(abs_builddir)/depends/x86_64-unknown-linux-gnu/include/boost/*" \
                      "$(abs_builddir)/depends/x86_64-unknown-linux-gnu/include/gmock/*" \
                      "$(abs_builddir)/depends/x86_64-unknown-linux-gnu/include/gtest/*" \
                      "$(abs_builddir)/src/gtest/*" \
                      "$(abs_builddir)/src/test/*" \
                      "$(abs_builddir)/src/wallet/gtest/*" \
                      "$(abs_builddir)/src/wallet/test/*" \
                      -o $@

test_bitcoin_coverage.info: baseline_filtered_combined.info test_bitcoin_filtered.info
	$(LCOV) -a baseline_filtered.info -a leveldb_baseline_filtered.info -a test_bitcoin_filtered.info -o $@

zcash-gtest_coverage.info: baseline_filtered_combined.info zcash-gtest_filtered.info
	$(LCOV) -a baseline_filtered.info -a leveldb_baseline_filtered.info -a zcash-gtest_filtered.info -o $@

total_coverage.info:  baseline_filtered_combined.info test_bitcoin_filtered.info zcash-gtest_filtered.info
	$(LCOV) -a baseline_filtered.info -a leveldb_baseline_filtered.info -a test_bitcoin_filtered.info -a zcash-gtest_filtered.info -o $@ | $(GREP) "\%" | $(AWK) '{ print substr($$3,2,50) "/" $$5 }' > coverage_percent.txt

test_bitcoin.coverage/.dirstamp:  test_bitcoin_coverage.info
	$(GENHTML) -s $< -o $(@D)
	@touch $@

zcash-gtest.coverage/.dirstamp:  zcash-gtest_coverage.info
	$(GENHTML) -s $< -o $(@D)
	@touch $@

cov-zcash: zcash-gtest.coverage/.dirstamp

total.coverage/.dirstamp: total_coverage.info
	$(GENHTML) -s $< -o $(@D)
	@touch $@

cov: test_bitcoin.coverage/.dirstamp cov-zcash total.coverage/.dirstamp

endif

dist_bin_SCRIPTS = zcutil/fetch-params.sh
dist_noinst_SCRIPTS = autogen.sh zcutil/build-debian-package.sh zcutil/build.sh

EXTRA_DIST = $(top_srcdir)/share/genbuild.sh qa/pull-tester/rpc-tests.sh qa/pull-tester/run-bitcoin-cli qa/rpc-tests qa/zcash $(DIST_DOCS) $(BIN_CHECKS)

install-exec-hook:
	mv $(DESTDIR)$(bindir)/fetch-params.sh $(DESTDIR)$(bindir)/zcash-fetch-params

.INTERMEDIATE: $(COVERAGE_INFO)

DISTCHECK_CONFIGURE_FLAGS = --enable-man

clean-local:
	rm -rf test_bitcoin.coverage/ zcash-gtest.coverage/ total.coverage/
