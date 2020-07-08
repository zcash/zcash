package=crate_thiserror_impl
$(package)_crate_name=thiserror-impl
$(package)_version=1.0.20
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=bd80fc12f73063ac132ac92aceea36734f04a1d93c1240c6944e23a3b8841793
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
