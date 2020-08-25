package=crate_version_check
$(package)_crate_name=version_check
$(package)_version=0.9.2
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=b5a972e5669d67ba988ce3dc826706fb0a8b01471c088cb0b6110b805cc36aed
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
