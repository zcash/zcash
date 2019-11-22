package=crate_bech32
$(package)_crate_name=bech32
$(package)_version=0.7.1
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=9e0089c35ab7c6f2bc55ab23f769913f0ac65b1023e7e74638a1f43128dd5df2
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
