package=libsodium
$(package)_version=1.0.20
$(package)_download_path=https://download.libsodium.org/libsodium/releases/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=ebb65ef6ca439333c2bb41a0c1990587288da07f6c7fd07cb3a18cc18d30ce19
$(package)_dependencies=
$(package)_patches=1.0.15-pubkey-validation.diff 1.0.15-signature-validation.diff
$(package)_config_opts=

define $(package)_preprocess_cmds
  patch -p1 < $($(package)_patch_dir)/1.0.15-pubkey-validation.diff && \
  patch -p1 < $($(package)_patch_dir)/1.0.15-signature-validation.diff && \
  cd $($(package)_build_subdir); DO_NOT_UPDATE_CONFIG_SCRIPTS=1 ./autogen.sh
endef

define $(package)_config_cmds
  $($(package)_autoconf) --enable-static --disable-shared
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
