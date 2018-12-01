package=crate_fuchsia_zircon
$(package)_crate_name=fuchsia-zircon
$(package)_version=0.3.3
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=2e9763c69ebaae630ba35f74888db465e49e259ba1bc0eda7d06f4a067615d82
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
