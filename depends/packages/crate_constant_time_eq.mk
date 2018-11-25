package=crate_constant_time_eq
$(package)_crate_name=constant_time_eq
$(package)_version=0.1.3
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=8ff012e225ce166d4422e0e78419d901719760f62ae2b7969ca6b564d1b54a9e
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
