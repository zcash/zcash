package=crate_typenum
$(package)_crate_name=typenum
$(package)_version=1.10.0
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=612d636f949607bdf9b123b4a6f6d966dedf3ff669f7f045890d3a4a73948169
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
