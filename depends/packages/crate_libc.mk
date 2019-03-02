package=crate_libc
$(package)_crate_name=libc
$(package)_version=0.2.40
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=6fd41f331ac7c5b8ac259b8bf82c75c0fb2e469bbf37d2becbba9a6a2221965b
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
