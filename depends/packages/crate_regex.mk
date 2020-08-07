package=crate_regex
$(package)_crate_name=regex
$(package)_version=1.3.9
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=9c3780fcf44b193bc4d09f36d2a3c87b251da4a046c87795a0d35f4f927ad8e6
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
