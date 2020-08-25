package=crate_maybe_uninit
$(package)_crate_name=maybe-uninit
$(package)_version=2.0.0
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=60302e4db3a61da70c0cb7991976248362f30319e88850c487b9b95bbf059e00
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
