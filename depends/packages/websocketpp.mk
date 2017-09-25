package=websocketpp
$(package)_version=0.7.0

$(package)_download_path=https://github.com/zaphoyd/$(package)/archive/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_download_file=$($(package)_version).tar.gz
$(package)_sha256_hash=07b3364ad30cda022d91759d4b83ff902e1ebadb796969e58b59caa535a03923

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_dir)$(host_prefix)/include && \
  cp -a ./websocketpp $($(package)_staging_dir)$(host_prefix)/include
endef

