package=crate_winapi
$(package)_crate_name=winapi
$(package)_version=0.3.4
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=04e3bd221fcbe8a271359c04f21a76db7d0c6028862d1bb5512d85e1e2eb5bb3
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
