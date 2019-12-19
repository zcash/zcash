package=crate_blake2b_simd
$(package)_crate_name=blake2b_simd
$(package)_version=0.5.9
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=b83b7baab1e671718d78204225800d6b170e648188ac7dc992e9d6bddf87d0c0
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
