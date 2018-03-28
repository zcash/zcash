package=crate_libc
$(package)_crate_name=libc
$(package)_version=0.2.21
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=88ee81885f9f04bff991e306fea7c1c60a5f0f9e409e99f6b40e3311a3363135
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
