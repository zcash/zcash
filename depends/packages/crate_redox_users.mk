package=crate_redox_users
$(package)_crate_name=redox_users
$(package)_version=0.3.4
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=09b23093265f8d200fa7b4c2c76297f47e681c655f6f1285a8780d6a022f7431
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
