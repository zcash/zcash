package=crate_ed25519_zebra
$(package)_crate_name=ed25519-zebra
$(package)_version=2.1.2
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=fc69a9bf9de8ad6cfa9c32db73dbe06ace3eb9a50a2f8c8520d8f453e13ae32a
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
