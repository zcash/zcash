package=crate_zcash_primitives
$(package)_crate_name=zcash_primitives
$(package)_version=0.3.0
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=c00f10013279ae11155d41b29a0d366012d4ed8c1a1886d72c247b244eb2adbc
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
