package=tl_expected
$(package)_version=1.3.1
$(package)_download_path=https://github.com/TartanLlama/expected/archive/refs/tags
$(package)_download_file=v$($(package)_version).tar.gz
$(package)_file_name=$(package)_$($(package)_version).tar.gz
$(package)_sha256_hash=9a04f4f472fbb5c30bf60402f1ca626c4a76987f867978d0b8a35d7ab3fb8fe7

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_dir)$(host_prefix)/include && \
  cp -r include/tl $($(package)_staging_dir)$(host_prefix)/include
endef
