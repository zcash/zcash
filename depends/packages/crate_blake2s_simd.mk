package=crate_blake2s_simd
$(package)_crate_name=blake2s_simd
$(package)_version=0.5.8
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=979da0ce13c897d6be19e005ea77ac12b0fea0157aeeee7feb8c49f91386f0ea
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
