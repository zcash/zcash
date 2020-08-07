package=crate_aho_corasick
$(package)_crate_name=aho-corasick
$(package)_version=0.7.13
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=043164d8ba5c4c3035fec9bbee8647c0261d788f3474306f93bb65901cae0e86
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
