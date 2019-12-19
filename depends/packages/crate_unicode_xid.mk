package=crate_unicode_xid
$(package)_crate_name=unicode_xid
$(package)_version=1.0.11
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=ea8fdee423bd624ae67512c036df78101025a3ef4d1caf9dafcdbb950845dac7
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
