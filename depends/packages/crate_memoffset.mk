package=crate_memoffset
$(package)_crate_name=memoffset
$(package)_version=0.5.5
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=c198b026e1bbf08a937e94c6c60f9ec4a2267f5b0d2eec9c1b21b061ce2be55f
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
