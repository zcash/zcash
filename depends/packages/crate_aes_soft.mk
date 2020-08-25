package=crate_aes_soft
$(package)_crate_name=aes-soft
$(package)_version=0.5.0
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=63dd91889c49327ad7ef3b500fd1109dbd3c509a03db0d4a9ce413b79f575cb6
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
