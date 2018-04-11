package=crate_crossbeam
$(package)_crate_name=crossbeam
$(package)_version=0.3.2
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=24ce9782d4d5c53674646a6a4c1863a21a8fc0cb649b3c94dfc16e45071dea19
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
