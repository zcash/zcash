package=utfcpp
$(package)_version=3.2.5
$(package)_download_path=https://github.com/nemtrif/$(package)/archive/refs/tags
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_download_file=v$($(package)_version).tar.gz
$(package)_sha256_hash=14fd1b3c466814cb4c40771b7f207b61d2c7a0aa6a5e620ca05c00df27f25afd

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_dir)$(host_prefix)/include && \
  cp -a ./source $($(package)_staging_dir)$(host_prefix)/include/utf8cpp
endef
