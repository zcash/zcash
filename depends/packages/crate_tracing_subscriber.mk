package=crate_tracing_subscriber
$(package)_crate_name=tracing-subscriber
$(package)_version=0.2.10
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=f7b33f8b2ef2ab0c3778c12646d9c42a24f7772bee4cdafc72199644a9f58fdc
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
