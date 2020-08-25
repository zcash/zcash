package=crate_constant_time_eq
$(package)_crate_name=constant_time_eq
$(package)_version=0.1.5
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=245097e9a4535ee1e3e3931fcfcd55a796a44c643e8596ff6566d68f09b87bbc
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
