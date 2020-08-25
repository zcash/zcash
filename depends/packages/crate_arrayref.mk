package=crate_arrayref
$(package)_crate_name=arrayref
$(package)_version=0.3.6
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=a4c527152e37cf757a3f78aae5a06fbeefdb07ccc535c980a3208ee3060dd544
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
