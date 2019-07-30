package=crate_typenum
$(package)_crate_name=typenum
$(package)_version=1.11.2
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=6d2783fe2d6b8c1101136184eb41be8b1ad379e4657050b8aaff0c79ee7575f9
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
