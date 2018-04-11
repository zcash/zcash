package=crate_pairing
$(package)_crate_name=pairing
$(package)_version=0.14.1
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=06f21a403a78257de696b59a5bfafad56a3b3ab8f27741c8122750bf0ebbb9fa
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
