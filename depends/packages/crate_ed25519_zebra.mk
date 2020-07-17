package=crate_ed25519_zebra
$(package)_crate_name=ed25519-zebra
$(package)_version=2.0.0
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=beeba2b02b91dc7cc2d1f42c96b9e82db4cd20ad1c326f1dbf64ac8943c7bf32
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
