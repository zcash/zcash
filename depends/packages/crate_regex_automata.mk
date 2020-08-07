package=crate_regex_automata
$(package)_crate_name=regex-automata
$(package)_version=0.1.9
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=ae1ded71d66a4a97f5e961fd0cb25a5f366a42a41570d16a763a69c092c26ae4
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
