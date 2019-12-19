package=crate_syn
$(package)_crate_name=syn
$(package)_version=1.0.11
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=dff0acdb207ae2fe6d5976617f887eb1e35a2ba52c13c7234c790960cdad9238
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
