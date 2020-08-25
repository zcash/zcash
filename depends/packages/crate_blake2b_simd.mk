package=crate_blake2b_simd
$(package)_crate_name=blake2b_simd
$(package)_version=0.5.10
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=d8fb2d74254a3a0b5cac33ac9f8ed0e44aa50378d9dbb2e5d83bd21ed1dc2c8a
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
