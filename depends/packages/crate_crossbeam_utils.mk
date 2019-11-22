package=crate_crossbeam_utils
$(package)_crate_name=crossbeam-utils
$(package)_version=0.6.6
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=04973fa96e96579258a5091af6003abde64af786b860f18622b82e026cca60e6
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
