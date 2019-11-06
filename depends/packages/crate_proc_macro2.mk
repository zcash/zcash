package=crate_proc_macro2
$(package)_crate_name=proc-macro2
$(package)_version=1.0.3
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=e98a83a9f9b331f54b924e68a66acb1bb35cb01fb0a23645139967abefb697e8
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
