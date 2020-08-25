package=crate_block_modes
$(package)_crate_name=block-modes
$(package)_version=0.6.1
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=0c9b14fd8a4739e6548d4b6018696cf991dcf8c6effd9ef9eb33b29b8a650972
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
