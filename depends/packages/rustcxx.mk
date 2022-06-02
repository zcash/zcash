package=rustcxx
$(package)_version=$(native_cxxbridge_version)
$(package)_file_name=$(native_cxxbridge_file_name)
$(package)_sha256_hash=$(native_cxxbridge_sha256_hash)

# Nothing to do, this was fetched by native_cxxbridge.
define $(package)_fetch_cmds
endef

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_prefix_dir)/include/rust && \
  cp include/cxx.h $($(package)_staging_prefix_dir)/include/rust
endef
