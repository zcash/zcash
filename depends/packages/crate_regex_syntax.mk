package=crate_regex_syntax
$(package)_crate_name=regex-syntax
$(package)_version=0.6.18
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=26412eb97c6b088a6997e05f69403a802a92d520de2f8e63c2b65f9e0f47c4e8
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
