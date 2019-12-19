package=crate_generic_array
$(package)_crate_name=generic-array
$(package)_version=0.13.2
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=0ed1e761351b56f54eb9dcd0cfaca9fd0daecf93918e1cfc01c8a3d26ee7adcd
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
