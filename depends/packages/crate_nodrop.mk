package=crate_nodrop
$(package)_crate_name=nodrop
$(package)_version=0.1.14
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=72ef4a56884ca558e5ddb05a1d1e7e1bfd9a68d9ed024c21704cc98872dae1bb
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
