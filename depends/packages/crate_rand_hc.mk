package=crate_rand_hc
$(package)_crate_name=rand_hc
$(package)_version=0.2.0
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=ca3129af7b92a17112d59ad498c6f81eaf463253766b90396d39ea7a39d6613c
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
