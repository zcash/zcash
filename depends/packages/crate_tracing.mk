package=crate_tracing
$(package)_crate_name=tracing
$(package)_version=0.1.18
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=f0aae59226cf195d8e74d4b34beae1859257efb4e5fed3f147d2dc2c7d372178
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
