package=native_cmake
$(package)_version=3.30.1
$(package)_download_path=https://github.com/Kitware/CMake/releases/download/v$($(package)_version)
$(package)_file_name=cmake-$($(package)_version).tar.gz
$(package)_sha256_hash=df9b3c53e3ce84c3c1b7c253e5ceff7d8d1f084ff0673d048f260e04ccb346e1

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
