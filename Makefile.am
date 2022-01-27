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


BITCOIND_BIN=$(top_builddir)/src/komodod$(EXEEXT)
BITCOIN_CLI_BIN=$(top_builddir)/src/komodo-cli$(EXEEXT)
#WALLET_UTILITY_BIN=$(top_builddir)/src/wallet-utility$(EXEEXT)
BITCOIN_WIN_INSTALLER=$(PACKAGE)-$(PACKAGE_VERSION)-win$(WINDOWS_BITS)-setup$(EXEEXT)

if TARGET_DARWIN
OSX_APP=Agama.app
OSX_DMG=Agama.dmg
OSX_BACKGROUND_IMAGE=background.tiff
OSX_DEPLOY_SCRIPT=$(top_srcdir)/contrib/macdeploy/macdeployqtplus
OSX_FANCY_PLIST=$(top_srcdir)/contrib/macdeploy/fancy.plist
OSX_BASE_LPROJ_DIR=$(top_srcdir)/contrib/macdeploy/Base.lproj/InfoPlist.strings
OSX_INSTALLER_ICONS=$(top_srcdir)/src/qt/res/icons/bitcoin.icns
OSX_PLIST=$(top_srcdir)/share/qt/Info.plist #not installed
OSX_QT_TRANSLATIONS = da,de,es,hu,ru,uk,zh_CN,zh_TW
endif

DIST_DOCS = $(wildcard doc/*.md) $(wildcard doc/release-notes/*.md)

BIN_CHECKS=$(top_srcdir)/contrib/devtools/symbol-check.py \
	$(top_srcdir)/contrib/devtools/security-check.py



WINDOWS_PACKAGING = $(top_srcdir)/share/pixmaps/bitcoin.ico \
	$(top_srcdir)/share/pixmaps/nsis-header.bmp \
	$(top_srcdir)/share/pixmaps/nsis-wizard.bmp

if TARGET_DARWIN
OSX_PACKAGING = $(OSX_DEPLOY_SCRIPT) $(OSX_FANCY_PLIST) $(OSX_INSTALLER_ICONS) $(OSX_BASE_LPROJ_DIR) \
	$(top_srcdir)/contrib/macdeploy/$(OSX_BACKGROUND_IMAGE) \
	$(top_srcdir)/contrib/macdeploy/DS_Store \
	$(top_srcdir)/contrib/macdeploy/detached-sig-apply.sh \
	$(top_srcdir)/contrib/macdeploy/detached-sig-create.sh
endif

if TARGET_DARWIN
COVERAGE_INFO = baseline_filtered_combined.info baseline.info block_test.info \
	leveldb_baseline.info test_bitcoin_filtered.info total_coverage.info \
	baseline_filtered.info block_test_filtered.info \
	leveldb_baseline_filtered.info test_bitcoin_coverage.info test_bitcoin.info
#  zcash-gtest.info zcash-gtest_filtered.info zcash-gtest_coverage.info
else
COVERAGE_INFO = baseline_filtered_combined.info baseline.info block_test.info \
  leveldb_baseline.info test_bitcoin_filtered.info total_coverage.info \
  baseline_filtered.info \
  leveldb_baseline_filtered.info test_bitcoin_coverage.info test_bitcoin.info \
  #zcash-gtest.info zcash-gtest_filtered.info zcash-gtest_coverage.info
endif

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

if TARGET_DARWIN

$(OSX_APP)/Contents/PkgInfo:
	$(MKDIR_P) $(@D)
	@echo "APPL????" > $@

$(OSX_APP)/Contents/Resources/empty.lproj:
	$(MKDIR_P) $(@D)
	@touch $@

$(OSX_APP)/Contents/Info.plist: $(OSX_PLIST)
	$(MKDIR_P) $(@D)
	$(INSTALL_DATA) $< $@

$(OSX_APP)/Contents/Resources/bitcoin.icns: $(OSX_INSTALLER_ICONS)
	$(MKDIR_P) $(@D)
	$(INSTALL_DATA) $< $@

$(OSX_APP)/Contents/MacOS/Bitcoin-Qt: $(BITCOIN_QT_BIN)
	$(MKDIR_P) $(@D)
	STRIPPROG="$(STRIP)" $(INSTALL_STRIP_PROGRAM)  $< $@

$(OSX_APP)/Contents/Resources/Base.lproj/InfoPlist.strings: $(OSX_BASE_LPROJ_DIR)
	$(MKDIR_P) $(@D)
	$(INSTALL_DATA) $< $@

OSX_APP_BUILT=$(OSX_APP)/Contents/PkgInfo $(OSX_APP)/Contents/Resources/empty.lproj \
	$(OSX_APP)/Contents/Resources/bitcoin.icns $(OSX_APP)/Contents/Info.plist \
	$(OSX_APP)/Contents/MacOS/Bitcoin-Qt $(OSX_APP)/Contents/Resources/Base.lproj/InfoPlist.strings

endif

if BUILD_DARWIN
$(OSX_DMG): $(OSX_APP_BUILT) $(OSX_PACKAGING)
	$(OSX_DEPLOY_SCRIPT) $(OSX_APP) -add-qt-tr $(OSX_QT_TRANSLATIONS) -translations-dir=$(QT_TRANSLATION_DIR) -dmg -fancy $(OSX_FANCY_PLIST) -verbose 2
deploydir: $(OSX_DMG)
else
APP_DIST_DIR=$(top_builddir)/dist
APP_DIST_EXTRAS=$(APP_DIST_DIR)/.background/$(OSX_BACKGROUND_IMAGE) $(APP_DIST_DIR)/.DS_Store $(APP_DIST_DIR)/Applications
endif

if TARGET_DARWIN
$(APP_DIST_DIR)/Applications:
	@rm -f $@
	@cd $(@D); $(LN_S) /Applications $(@F)

$(APP_DIST_EXTRAS): $(APP_DIST_DIR)/$(OSX_APP)/Contents/MacOS/Bitcoin-Qt

$(OSX_DMG): $(APP_DIST_EXTRAS)
	$(GENISOIMAGE) -no-cache-inodes -D -l -probe -V "Bitcoin-Core" -no-pad -r -apple -o $@ dist

$(APP_DIST_DIR)/.background/$(OSX_BACKGROUND_IMAGE): contrib/macdeploy/$(OSX_BACKGROUND_IMAGE)
	$(MKDIR_P) $(@D)
	$(INSTALL) $< $@
$(APP_DIST_DIR)/.DS_Store: contrib/macdeploy/DS_Store
	$(INSTALL) $< $@

$(APP_DIST_DIR)/$(OSX_APP)/Contents/MacOS/Bitcoin-Qt: $(OSX_APP_BUILT) $(OSX_PACKAGING)
	INSTALLNAMETOOL=$(INSTALLNAMETOOL)  OTOOL=$(OTOOL) STRIP=$(STRIP) $(OSX_DEPLOY_SCRIPT) $(OSX_APP) -translations-dir=$(QT_TRANSLATION_DIR) -add-qt-tr $(OSX_QT_TRANSLATIONS) -verbose 2

deploydir: $(APP_DIST_EXTRAS)
endif

if TARGET_DARWIN
appbundle: $(OSX_APP_BUILT)
deploy: $(OSX_DMG)
endif

if TARGET_WINDOWS
deploy: $(BITCOIN_WIN_INSTALLER)
endif

$(BITCOIND_BIN): FORCE
	$(MAKE) -C src $(@F)

$(BITCOIN_CLI_BIN): FORCE
	$(MAKE) -C src $(@F)

#$(WALLET_UTILITY_BIN): FORCE
#	$(MAKE) -C src $(@F)

if USE_LCOV

baseline.info:
	$(LCOV) -c -i -d $(abs_builddir)/src -o $@

if BUILD_DARWIN
baseline_filtered.info: baseline.info
	$(LCOV) -r $< "/usr/include/*" \
	"$(abs_builddir)/depends/$(BUILD)/include/*.h" \
	"$(abs_builddir)/depends/$(BUILD)/include/boost/*" \
	"$(abs_builddir)/depends/$(BUILD)/include/gmock/*" \
	"$(abs_builddir)/depends/$(BUILD)/include/gtest/*" \
	"$(abs_builddir)/src/gtest/*" \
	"$(abs_builddir)/src/test/*" \
	"$(abs_builddir)/src/wallet/gtest/*" \
	"$(abs_builddir)/src/wallet/test/*" \
	-o $@
else
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
endif

leveldb_baseline.info: baseline_filtered.info
	$(LCOV) -c -i -d $(abs_builddir)/src/leveldb -b $(abs_builddir)/src/leveldb -o $@

if BUILD_DARWIN
leveldb_baseline_filtered.info: leveldb_baseline.info
	$(LCOV) -r $< "/usr/include/*" \
	"$(abs_builddir)/depends/$(BUILD)/include/*.h" \
	"$(abs_builddir)/depends/$(BUILD)/include/boost/*" \
	"$(abs_builddir)/depends/$(BUILD)/include/gmock/*" \
	"$(abs_builddir)/depends/$(BUILD)/include/gtest/*" \
	"$(abs_builddir)/src/gtest/*" \
	"$(abs_builddir)/src/test/*" \
	"$(abs_builddir)/src/wallet/gtest/*" \
	"$(abs_builddir)/src/wallet/test/*" \
	-o $@
else
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
endif

baseline_filtered_combined.info: leveldb_baseline_filtered.info baseline_filtered.info
	$(LCOV) -a leveldb_baseline_filtered.info -a baseline_filtered.info -o $@

test_bitcoin.info: baseline_filtered_combined.info
	$(MAKE) -C src/ bitcoin_test_check
	$(LCOV) -c -d $(abs_builddir)/src -t test_bitcoin -o $@
	$(LCOV) -z -d $(abs_builddir)/src
	$(LCOV) -z -d $(abs_builddir)/src/leveldb

if BUILD_DARWIN
test_bitcoin_filtered.info: test_bitcoin.info
	$(LCOV) -r $< "/usr/include/*" \
	"$(abs_builddir)/depends/$(BUILD)/include/*.h" \
	"$(abs_builddir)/depends/$(BUILD)/include/boost/*" \
	"$(abs_builddir)/depends/$(BUILD)/include/gmock/*" \
	"$(abs_builddir)/depends/$(BUILD)/include/gtest/*" \
	"$(abs_builddir)/src/gtest/*" \
	"$(abs_builddir)/src/test/*" \
	"$(abs_builddir)/src/wallet/gtest/*" \
	"$(abs_builddir)/src/wallet/test/*" \
	-o $@
else
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
endif


block_test.info: test_bitcoin_filtered.info
	$(MKDIR_P) qa/tmp
	-@TIMEOUT=15 qa/pull-tester/run-bitcoind-for-test.sh $(JAVA) -jar $(JAVA_COMPARISON_TOOL) qa/tmp/compTool 0
	$(LCOV) -c -d $(abs_builddir)/src --t BitcoinJBlockTest -o $@
	$(LCOV) -z -d $(abs_builddir)/src
	$(LCOV) -z -d $(abs_builddir)/src/leveldb

if BUILD_DARWIN
block_test_filtered.info: block_test.info
	$(LCOV) -r $< "/usr/include/*" \
	"$(abs_builddir)/depends/$(BUILD)/include/*.h" \
	"$(abs_builddir)/depends/$(BUILD)/include/boost/*" \
	"$(abs_builddir)/depends/$(BUILD)/include/gmock/*" \
	"$(abs_builddir)/depends/$(BUILD)/include/gtest/*" \
	"$(abs_builddir)/src/gtest/*" \
	"$(abs_builddir)/src/test/*" \
	"$(abs_builddir)/src/wallet/gtest/*" \
	"$(abs_builddir)/src/wallet/test/*" \
	-o $@
else
block_test_filtered.info: block_test.info
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
endif


test_bitcoin_coverage.info: baseline_filtered_combined.info test_bitcoin_filtered.info
	$(LCOV) -a baseline_filtered.info -a leveldb_baseline_filtered.info -a test_bitcoin_filtered.info -o $@

if ! BUILD_DARWIN
zcash-gtest_coverage.info: baseline_filtered_combined.info zcash-gtest_filtered.info
	$(LCOV) -a baseline_filtered.info -a leveldb_baseline_filtered.info -a zcash-gtest_filtered.info -o $@
endif

if BUILD_DARWIN
total_coverage.info:  baseline_filtered_combined.info test_bitcoin_filtered.info zcash-gtest_filtered.info block_test_filtered.info
	$(LCOV) -a baseline_filtered.info -a leveldb_baseline_filtered.info -a test_bitcoin_filtered.info -a zcash-gtest_filtered.info -a block_test_filtered.info -o $@ | $(GREP) "\%" | $(AWK) '{ print substr($$3,2,50) "/" $$5 }' > coverage_percent.txt
else
total_coverage.info:  baseline_filtered_combined.info test_bitcoin_filtered.info zcash-gtest_filtered.info block_test_filtered.info
	$(LCOV) -a baseline_filtered.info -a leveldb_baseline_filtered.info -a test_bitcoin_filtered.info -a zcash-gtest_filtered.info -a block_test_filtered.info -o $@ | $(GREP) "\%" | $(AWK) '{ print substr($$3,2,50) "/" $$5 }' > coverage_percent.txt
endif


test_bitcoin.coverage/.dirstamp:  test_bitcoin_coverage.info
	$(GENHTML) -s $< -o $(@D)
	@touch $@

if TARGET_DARWIN
zcash-gtest.coverage/.dirstamp:  zcash-gtest_coverage.info
	$(GENHTML) -s $< -o $(@D)
	@touch $@
cov-zcash: zcash-gtest.coverage/.dirstamp
endif

total.coverage/.dirstamp: total_coverage.info
	$(GENHTML) -s $< -o $(@D)
	@touch $@

if BUILD_DARWIN
cov: test_bitcoin.coverage/.dirstamp cov-zcash total.coverage/.dirstamp
else
cov: test_bitcoin.coverage/.dirstamp cov-zcash total.coverage/.dirstamp
endif

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
