package=crate_sharded_slab
$(package)_crate_name=sharded-slab
$(package)_version=0.0.9
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=06d5a3f5166fb5b42a5439f2eee8b9de149e235961e3eb21c5808fc3ea17ff3e
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
