package=utfcpp
$(package)_version=4.0.6
$(package)_download_path=https://github.com/nemtrif/$(package)/archive/refs/tags
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_download_file=v$($(package)_version).tar.gz
$(package)_sha256_hash=6920a6a5d6a04b9a89b2a89af7132f8acefd46e0c2a7b190350539e9213816c0

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_dir)$(host_prefix)/include && \
  cp -a ./source $($(package)_staging_dir)$(host_prefix)/include/utf8cpp
endef
