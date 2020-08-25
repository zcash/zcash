package=crate_ff_derive
$(package)_crate_name=ff_derive
$(package)_version=0.7.0
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=6876796d97d4a5e7f1c4a1645c5111f3acb2fc924172b78d949b8a89eb970d6a
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
