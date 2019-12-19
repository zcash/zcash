package=crate_crossbeam_epoch
$(package)_crate_name=crossbeam_epoch
$(package)_version=0.8.0
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=715e8e65a0cf0106864f48c9d269d50acb53000e89a0db453196e55acd8f9cc4
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
