package=crate_serde
$(package)_crate_name=serde
$(package)_version=1.0.115
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=e54c9a88f2da7238af84b5101443f0c0d0a3bbdc455e34a5c9497b1903ed55d5
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
