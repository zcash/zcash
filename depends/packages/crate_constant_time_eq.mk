package=crate_constant_time_eq
$(package)_crate_name=constant_time_eq
$(package)_version=0.1.4
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=995a44c877f9212528ccc74b21a232f66ad69001e40ede5bcee2ac9ef2657120
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
