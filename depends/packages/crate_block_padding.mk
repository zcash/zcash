package=crate_block_padding
$(package)_crate_name=block-padding
$(package)_version=0.1.4
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=6d4dc3af3ee2e12f3e5d224e5e1e3d73668abbeb69e566d361f7d5563a4fdf09
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
