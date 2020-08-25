package=crate_opaque_debug
$(package)_crate_name=opaque-debug
$(package)_version=0.3.0
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=624a8340c38c1b80fd549087862da4ba43e08858af025b236e509b6649fc13d5
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
