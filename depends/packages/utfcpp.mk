package=utfcpp
$(package)_version=4.1.1
$(package)_download_path=https://github.com/nemtrif/$(package)/archive/refs/tags
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_download_file=v$($(package)_version).tar.gz
$(package)_sha256_hash=1ca68016f0abc24172998e39ce0d8f8e2b7a26f7579a0ff85d4e1b9a7aea56f8

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_dir)$(host_prefix)/include && \
  cp -a ./source $($(package)_staging_dir)$(host_prefix)/include/utf8cpp
endef
