package=crate_aes_soft
$(package)_crate_name=aes-soft
$(package)_version=0.3.3
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=cfd7e7ae3f9a1fb5c03b389fc6bb9a51400d0c13053f0dca698c832bfd893a0d
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
