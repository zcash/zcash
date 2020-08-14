package=crate_crossbeam_channel
$(package)_crate_name=crossbeam-channel
$(package)_version=0.4.3
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=09ee0cc8804d5393478d743b035099520087a5186f3b93fa58cec08fa62407b6
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
