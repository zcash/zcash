package=tl_expected
$(package)_version=1.1.0
$(package)_download_path=https://github.com/TartanLlama/expected/archive/refs/tags
$(package)_download_file=v$($(package)_version).tar.gz
$(package)_file_name=$(package)_$($(package)_version).tar.gz
$(package)_sha256_hash=1db357f46dd2b24447156aaf970c4c40a793ef12a8a9c2ad9e096d9801368df6

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_dir)$(host_prefix)/include && \
  cp -r include/tl $($(package)_staging_dir)$(host_prefix)/include
endef
