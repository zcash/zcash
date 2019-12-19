package=crate_blake2s_simd
$(package)_crate_name=blake2s-simd
$(package)_version=0.5.9
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=075022a6690070805a8bde2b355192454b2795a4b30a77543bf4da0c25d555ca
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
