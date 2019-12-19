package=crate_getrandom
$(package)_crate_name=getrandom
$(package)_version=0.1.13
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=e7db7ca94ed4cd01190ceee0d8a8052f08a247aa1b469a7f68c6a3b71afcf407
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
