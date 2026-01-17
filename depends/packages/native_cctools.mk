package=native_cctools
$(package)_version=2ef2e931cf641547eb8a68cfebde61003587c9fd
$(package)_download_path=https://github.com/tpoechtrager/cctools-port/archive
$(package)_download_file=$($(package)_version).tar.gz
$(package)_file_name=$(package)_$($(package)_version).tar.gz
$(package)_sha256_hash=6b73269efdf5c58a070e7357b66ee760501388549d6a12b423723f45888b074b
$(package)_build_subdir=cctools
$(package)_patches=ignore-otool.diff
$(package)_dependencies=native_clang native_libtapi

define $(package)_set_vars
  $(package)_config_opts=--target=$(host)
  $(package)_ldflags+=-Wl,-rpath=\\$$$$$$$$\$$$$$$$$ORIGIN/../lib
  $(package)_config_opts+=--enable-lto-support --with-llvm-config=$(build_prefix)/bin/llvm-config
  $(package)_cc=$(clang_prog)
  $(package)_cxx=$(clangxx_prog)
endef

define $(package)_preprocess_cmds
  patch -p1 < $($(package)_patch_dir)/ignore-otool.diff && \
  cd $($(package)_build_subdir); DO_NOT_UPDATE_CONFIG_SCRIPTS=1 ./autogen.sh
endef

define $(package)_config_cmds
  rm -f $(build_prefix)/lib/libc++abi.so* && \
  CC=$($(package)_cc) CXX=$($(package)_cxx) INSTALLPREFIX=$($(package)_extract_dir) ../libtapi/build.sh && \
  CC=$($(package)_cc) CXX=$($(package)_cxx) INSTALLPREFIX=$($(package)_extract_dir) ../libtapi/install.sh && \
  $($(package)_config_env) $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef

define $(package)_postprocess_cmds
  rm -rf share
endef
