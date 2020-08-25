package=crate_unicode_xid
$(package)_crate_name=unicode-xid
$(package)_version=0.2.1
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=f7fe0bb3479651439c9112f72b6c505038574c9fbb575ed1bf3b797fa39dd564
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
