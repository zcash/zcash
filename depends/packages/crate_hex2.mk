package=crate_hex2
$(package)_crate_name=hex
$(package)_version=0.4.2
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=644f9158b2f133fd50f5fb3242878846d9eb792e445c893805ff0e3824006e35
$(package)_crate_versioned_name="$($(package)_crate_name) 0.4.2"

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
