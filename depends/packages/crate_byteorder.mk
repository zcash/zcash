package=crate_byteorder
$(package)_crate_name=byteorder
$(package)_version=1.2.2
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=73b5bdfe7ee3ad0b99c9801d58807a9dbc9e09196365b0203853b99889ab3c87
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
