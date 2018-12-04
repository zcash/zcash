package=crate_arrayvec
$(package)_crate_name=arrayvec
$(package)_version=0.4.7
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=a1e964f9e24d588183fcb43503abda40d288c8657dfc27311516ce2f05675aef
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
