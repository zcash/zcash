package=crate_thiserror
$(package)_crate_name=thiserror
$(package)_version=1.0.20
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=7dfdd070ccd8ccb78f4ad66bf1982dc37f620ef696c6b5028fe2ed83dd3d0d08
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
