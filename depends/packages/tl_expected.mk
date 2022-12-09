package=tl_expected
$(package)_version=1.0.1
$(package)_download_path=https://github.com/TartanLlama/expected/archive
$(package)_download_file=96d547c03d2feab8db64c53c3744a9b4a7c8f2c5.tar.gz
$(package)_file_name=tl-expected-1.0.1.tar.gz
$(package)_sha256_hash=64901df1de9a5a3737b331d3e1de146fa6ffb997017368b322c08f45c51b90a7
$(package)_patches=remove-undefined-behaviour.diff

define $(package)_preprocess_cmds
  patch -p1 < $($(package)_patch_dir)/remove-undefined-behaviour.diff
endef

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_dir)$(host_prefix)/include && \
  cp -r include/tl $($(package)_staging_dir)$(host_prefix)/include
endef
