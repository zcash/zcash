package=crate_tracing_core
$(package)_crate_name=tracing-core
$(package)_version=0.1.15
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=4f0e00789804e99b20f12bc7003ca416309d28a6f495d6af58d1e2c2842461b5
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
