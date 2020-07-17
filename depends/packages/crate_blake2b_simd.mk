package=crate_blake2b_simd
$(package)_crate_name=blake2b_simd
$(package)_version=0.5.8
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=5850aeee1552f495dd0250014cf64b82b7c8879a89d83b33bbdace2cc4f63182
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
