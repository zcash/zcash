package=crate_num_bigint
$(package)_crate_name=num-bigint
$(package)_version=0.3.0
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=b7f3fc75e3697059fb1bc465e3d8cca6cf92f56854f201158b3f9c77d5a3cfa0
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
