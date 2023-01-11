package=utfcpp
$(package)_version=3.2.3
$(package)_download_path=https://github.com/nemtrif/$(package)/archive/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_download_file=v$($(package)_version).tar.gz
$(package)_sha256_hash=3ba9b0dbbff08767bdffe8f03b10e596ca351228862722e4c9d4d126d2865250

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_dir)$(host_prefix)/include && \
  cp -a ./source $($(package)_staging_dir)$(host_prefix)/include/utf8cpp
endef
