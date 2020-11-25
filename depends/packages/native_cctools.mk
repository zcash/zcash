package=native_cctools
$(package)_version=55562e4073dea0fbfd0b20e0bf69ffe6390c7f97
$(package)_download_path=https://github.com/tpoechtrager/cctools-port/archive
$(package)_file_name=$($(package)_version).tar.gz
$(package)_sha256_hash=e51995a843533a3dac155dd0c71362dd471597a2d23f13dff194c6285362f875
$(package)_build_subdir=cctools
$(package)_dependencies=native_clang
$(package)_patches=ignore-otool.diff

$(package)_libtapi_version=3efb201881e7a76a21e0554906cf306432539cef
$(package)_libtapi_download_path=https://github.com/tpoechtrager/apple-libtapi/archive
$(package)_libtapi_download_file=$($(package)_libtapi_version).tar.gz
$(package)_libtapi_file_name=$($(package)_libtapi_version).tar.gz
$(package)_libtapi_sha256_hash=380c1ca37cfa04a8699d0887a8d3ee1ad27f3d08baba78887c73b09485c0fbd3

$(package)_extra_sources += $($(package)_libtapi_file_name)

define $(package)_fetch_cmds
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_download_file),$($(package)_file_name),$($(package)_sha256_hash)) && \
$(call fetch_file,$(package),$($(package)_libtapi_download_path),$($(package)_libtapi_download_file),$($(package)_libtapi_file_name),$($(package)_libtapi_sha256_hash))
endef

define $(package)_extract_cmds
  mkdir -p $($(package)_extract_dir) && \
  echo "$($(package)_sha256_hash)  $($(package)_source)" > $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  echo "$($(package)_libtapi_sha256_hash)  $($(package)_source_dir)/$($(package)_libtapi_file_name)" >> $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  $(build_SHA256SUM) -c $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  mkdir -p libtapi && \
  tar --no-same-owner --strip-components=1 -C libtapi -xf $($(package)_source_dir)/$($(package)_libtapi_file_name) && \
  tar --no-same-owner --strip-components=1 -xf $($(package)_source)
endef

define $(package)_set_vars
  $(package)_config_opts=--target=$(host) --with-libtapi=$($(package)_extract_dir)
  $(package)_ldflags+=-Wl,-rpath=\\$$$$$$$$\$$$$$$$$ORIGIN/../lib
  $(package)_config_opts+=--enable-lto-support --with-llvm-config=$(build_prefix)/bin/llvm-config
  $(package)_cc=$(build_prefix)/bin/clang
  $(package)_cxx=$(build_prefix)/bin/clang++
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
  $(MAKE) DESTDIR=$($(package)_staging_dir) install && \
  mkdir -p $($(package)_staging_prefix_dir)/lib/ && \
  cd $($(package)_extract_dir) && \
  cp lib/libtapi.so.6 $($(package)_staging_prefix_dir)/lib/
endef
