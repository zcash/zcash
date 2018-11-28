package=crate_stream_cipher
$(package)_crate_name=stream-cipher
$(package)_version=0.1.1
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=30dc6118470d69ce0fdcf7e6f95e95853f7f4f72f80d835d4519577c323814ab
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
