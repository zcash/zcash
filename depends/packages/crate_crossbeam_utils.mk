package=crate_crossbeam_utils
$(package)_crate_name=crossbeam-utils
$(package)_version=0.7.2
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=c3c7c73a2d1e9fc0886a08b93e98eb643461230d5f1925e4036204d5f2e261a8
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
