package=utfcpp
$(package)_version=3.2.4
$(package)_download_path=https://github.com/nemtrif/$(package)/archive/refs/tags
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_download_file=v$($(package)_version).tar.gz
$(package)_sha256_hash=fde21a4c519eed25f095a1cd8490167409cc70d7b5e9c38756142e588ccb7c7e

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_dir)$(host_prefix)/include && \
  cp -a ./source $($(package)_staging_dir)$(host_prefix)/include/utf8cpp
endef
