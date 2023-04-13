package=native_cmake
$(package)_version=3.26.3
$(package)_download_path=https://github.com/Kitware/CMake/releases/download/v$($(package)_version)
$(package)_file_name=cmake-$($(package)_version).tar.gz
$(package)_sha256_hash=bbd8d39217509d163cb544a40d6428ac666ddc83e22905d3e52c925781f0f659

define $(package)_set_vars
$(package)_config_opts += -DCMAKE_BUILD_TYPE:STRING=Release
$(package)_config_opts += -DCMAKE_USE_OPENSSL:BOOL=OFF
endef

define $(package)_config_cmds
  ./bootstrap --prefix=$($(package)_staging_prefix_dir) -- $($(package)_config_opts)
endef

define $(package)_build_cmds
  $(MAKE) cmake
endef

define $(package)_stage_cmds
  $(MAKE) install
endef
