package=crate_serde_derive
$(package)_crate_name=serde_derive
$(package)_version=1.0.115
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=609feed1d0a73cc36a0182a840a9b37b4a82f0b1150369f0536a9e3f2a31dc48
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
