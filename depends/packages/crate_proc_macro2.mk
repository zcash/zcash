package=crate_proc_macro2
$(package)_crate_name=proc_macro2
$(package)_version=1.0.6
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=fa28395ed85f39a52ac3aab4415db9879baa40d2a93b4ef7f7b4a9f97e0a7877
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
