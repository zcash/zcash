package=crate_pairing
$(package)_crate_name=pairing
$(package)_version=0.17.0
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=8b6f13c321bf1105ef510ced10d0bc84ff9be6e8f593dd635af7a797b0335b91
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
