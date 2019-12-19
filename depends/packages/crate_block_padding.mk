package=crate_block_padding
$(package)_crate_name=block-padding
$(package)_version=0.1.5
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=fa79dedbb091f449f1f39e53edf88d5dbe95f895dae6135a8d7b881fb5af73f5
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
