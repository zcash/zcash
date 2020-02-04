package=crate_ff
$(package)_crate_name=ff
$(package)_version=0.5.0
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=44b4c77ad8a724f1ebb882af5d2d7a2ab62f4d63c8e401d40ab0de1d75262ea3
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
