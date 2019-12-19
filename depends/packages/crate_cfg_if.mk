package=crate_cfg_if
$(package)_crate_name=cfg_if
$(package)_version=0.1.10
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=5a2eb7be31a6e44c90e3f28c3c751358d647999f5b751ff606edd30f33fc3d61
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
