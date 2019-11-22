package=crate_autocfg
$(package)_crate_name=autocfg
$(package)_version=0.1.6
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=b671c8fb71b457dd4ae18c4ba1e59aa81793daacc361d82fcd410cef0d491875
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
