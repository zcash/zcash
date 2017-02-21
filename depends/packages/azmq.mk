package=azmq
$(package)_version=1.0
$(package)_download_path=https://github.com/zeromq/$(package)/archive/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_download_file=v$($(package)_version).tar.gz
$(package)_sha256_hash=c204c731bcb7810ca3a2c5515e88974ef2ff8d0589e60a897dc238b369180e7b
$(package)_dependencies=boost zeromq
$(package)_patches=libsodium.patch

define $(package)_preprocess_cmds
  patch -p1 < $($(package)_patch_dir)/libsodium.patch && \
  mkdir build
endef

define $(package)_config_cmds
  cd build; cmake .. -DCMAKE_INSTALL_PREFIX=/ -DBOOST_ROOT=$(host_prefix) -DZMQ_ROOT=$(host_prefix)
endef

define $(package)_build_cmds
  cd build; $(MAKE) VERBOSE=1
endef

define $(package)_stage_cmds
  cd build; $(MAKE) VERBOSE=1 DESTDIR=$($(package)_staging_prefix_dir) install
endef
