package=crate_getrandom
$(package)_crate_name=getrandom
$(package)_version=0.1.14
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=7abc8dd8451921606d809ba32e95b6111925cd2906060d2dcc29c070220503eb
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
