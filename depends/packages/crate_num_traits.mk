package=crate_num_traits
$(package)_crate_name=num-traits
$(package)_version=0.2.12
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=ac267bcc07f48ee5f8935ab0d24f316fb722d7a1292e2913f0cc196b29ffd611
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
