package=crate_crossbeam_epoch
$(package)_crate_name=crossbeam-epoch
$(package)_version=0.8.0
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=5064ebdbf05ce3cb95e45c8b086f72263f4166b29b97f6baff7ef7fe047b55ac
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
