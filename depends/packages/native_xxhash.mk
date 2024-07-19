package=native_xxhash
$(package)_version=0.8.2
$(package)_download_path=https://github.com/Cyan4973/xxHash/archive/refs/tags
$(package)_download_file=v$($(package)_version).tar.gz
$(package)_file_name=xxhash-$($(package)_version).tar.gz
$(package)_sha256_hash=baee0c6afd4f03165de7a4e67988d16f0f2b257b51d0e3cb91909302a26a79c4

define $(package)_build_cmds
  $(MAKE) libxxhash.a
endef

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_prefix_dir)/include && \
  mkdir -p $($(package)_staging_prefix_dir)/lib && \
  cp xxhash.h $($(package)_staging_prefix_dir)/include && \
  cp libxxhash.a $($(package)_staging_prefix_dir)/lib
endef
