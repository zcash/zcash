package=tl_expected
$(package)_version=1.2.0
$(package)_download_path=https://github.com/TartanLlama/expected/archive/refs/tags
$(package)_download_file=v$($(package)_version).tar.gz
$(package)_file_name=$(package)_$($(package)_version).tar.gz
$(package)_sha256_hash=f5424f5fc74e79157b9981ba2578a28e0285ac6ec2a8f075e86c41226fe33386

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_dir)$(host_prefix)/include && \
  cp -r include/tl $($(package)_staging_dir)$(host_prefix)/include
endef
