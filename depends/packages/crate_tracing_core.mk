package=crate_tracing_core
$(package)_crate_name=tracing-core
$(package)_version=0.1.12
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=b2734b5a028fa697686f16c6d18c2c6a3c7e41513f9a213abb6754c4acb3c8d7
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
