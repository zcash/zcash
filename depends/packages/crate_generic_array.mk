package=crate_generic_array
$(package)_crate_name=generic-array
$(package)_version=0.14.4
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=501466ecc8a30d1d3b7fc9229b122b2ce8ed6e9d9223f1138d4babb253e51817
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
