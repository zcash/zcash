package=crate_num_traits
$(package)_crate_name=num_traits
$(package)_version=0.2.10
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=a31002d6ad3f4d3446512fe703d0d83610902e706ed423d556ebb7cd9ebd3bf2
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
