package=crate_blake2s_simd
$(package)_crate_name=blake2s_simd
$(package)_version=0.5.9
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=050efd7a5bdb220988d4c5204f84ab796e778612af94275f1d39479798b39cf9
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
