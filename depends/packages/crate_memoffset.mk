package=crate_memoffset
$(package)_crate_name=memoffset
$(package)_version=0.5.1
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=ce6075db033bbbb7ee5a0bbd3a3186bbae616f57fb001c485c7ff77955f8177f
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
