package=crate_ppv_lite86
$(package)_crate_name=ppv-lite86
$(package)_version=0.2.9
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=c36fa947111f5c62a733b652544dd0016a43ce89619538a8ef92724a6f501a20
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
