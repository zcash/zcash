package=crate_tracing_appender
$(package)_crate_name=tracing-appender
$(package)_version=0.1.1
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=7aa52d56cc0d79ab604e8a022a1cebc4de33cf09dc9933c94353bea2e00d6e88
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
