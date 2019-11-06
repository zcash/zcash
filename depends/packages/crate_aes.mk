package=crate_aes
$(package)_crate_name=aes
$(package)_version=0.3.2
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=54eb1d8fe354e5fc611daf4f2ea97dd45a765f4f1e4512306ec183ae2e8f20c9
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
