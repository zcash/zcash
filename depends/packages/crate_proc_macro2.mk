package=crate_proc_macro2
$(package)_crate_name=proc-macro2
$(package)_version=1.0.19
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=04f5f085b5d71e2188cb8271e5da0161ad52c3f227a661a3c135fdf28e258b12
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
