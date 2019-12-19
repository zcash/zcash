package=crate_ppv_lite86
$(package)_crate_name=ppv-lite86
$(package)_version=0.2.6
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=74490b50b9fbe561ac330df47c08f3f33073d2d00c150f719147d7c54522fa1b
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
