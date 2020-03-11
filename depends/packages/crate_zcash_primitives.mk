package=crate_zcash_primitives
$(package)_crate_name=zcash_primitives
$(package)_version=0.1.0
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=9530749bc784c4ca0d7bf000333cec29acf94f1875ad8db088e12dfee1095d13
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
