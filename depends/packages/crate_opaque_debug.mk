package=crate_opaque_debug
$(package)_crate_name=opaque-debug
$(package)_version=0.1.1
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=d620c9c26834b34f039489ac0dfdb12c7ac15ccaf818350a64c9b5334a452ad7
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
