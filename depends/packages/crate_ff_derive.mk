package=crate_ff_derive
$(package)_crate_name=ff_derive
$(package)_version=0.6.0
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=a3776aaf60a45037a9c3cabdd8542b38693acaa3e241ff957181b72579d29feb
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
