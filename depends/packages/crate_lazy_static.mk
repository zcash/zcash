package=crate_lazy_static
$(package)_crate_name=lazy_static
$(package)_version=1.4.0
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=e2abad23fbc42b3700f2f279844dc832adb2b2eb069b2df918f455c4e18cc646
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
