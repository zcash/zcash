package=crate_tracing_attributes
$(package)_crate_name=tracing-attributes
$(package)_version=0.1.11
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=80e0ccfc3378da0cce270c946b676a376943f5cd16aeba64568e7939806f4ada
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
