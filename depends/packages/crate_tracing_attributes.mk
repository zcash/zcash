package=crate_tracing_attributes
$(package)_crate_name=tracing-attributes
$(package)_version=0.1.9
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=f0693bf8d6f2bf22c690fc61a9d21ac69efdbb894a17ed596b9af0f01e64b84b
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
