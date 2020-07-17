package=crate_serde_derive
$(package)_crate_name=serde_derive
$(package)_version=1.0.113
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=93c5eaa17d0954cb481cdcfffe9d84fcfa7a1a9f2349271e678677be4c26ae31
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
