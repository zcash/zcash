package=crate_tracing_subscriber
$(package)_crate_name=tracing-subscriber
$(package)_version=0.2.11
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=abd165311cc4d7a555ad11cc77a37756df836182db0d81aac908c8184c584f40
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
