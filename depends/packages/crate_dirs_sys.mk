package=crate_dirs_sys
$(package)_crate_name=dirs-sys
$(package)_version=0.3.5
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=8e93d7f5705de3e49895a2b5e0b8855a1c27f080192ae9c32a6432d50741a57a
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
