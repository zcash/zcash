package=crate_block_cipher_trait
$(package)_crate_name=block-cipher-trait
$(package)_version=0.5.3
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=370424437b9459f3dfd68428ed9376ddfe03d8b70ede29cc533b3557df186ab4
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
