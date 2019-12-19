package=crate_crossbeam
$(package)_crate_name=crossbeam
$(package)_version=0.7.3
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=69323bff1fb41c635347b8ead484a5ca6c3f11914d784170b158d8449ab07f8e
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
