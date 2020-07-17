package=crate_zcash_history
$(package)_crate_name=zcash_history
$(package)_version=0.2.0
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=abfbab9accba014bbf3098d5aa66c1714d0db4abe25b999b8400bbd626ccd2f4
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
