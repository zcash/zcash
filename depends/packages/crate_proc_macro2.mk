package=crate_proc_macro2
$(package)_crate_name=proc-macro2
$(package)_version=1.0.6
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=9c9e470a8dc4aeae2dee2f335e8f533e2d4b347e1434e5671afc49b054592f27
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
