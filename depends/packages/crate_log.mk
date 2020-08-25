package=crate_log
$(package)_crate_name=log
$(package)_version=0.4.11
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=4fabed175da42fed1fa0746b0ea71f412aa9d35e76e95e59b192c64b9dc2bf8b
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
