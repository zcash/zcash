package=crate_rust_argon2
$(package)_crate_name=rust-argon2
$(package)_version=0.7.0
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=2bc8af4bda8e1ff4932523b94d3dd20ee30a87232323eda55903ffd71d2fb017
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
