package=crate_syn
$(package)_crate_name=syn
$(package)_version=1.0.39
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=891d8d6567fe7c7f8835a3a98af4208f3846fba258c1bc3c31d6e506239f11f9
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
