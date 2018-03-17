package=librustzcash
$(package)_version=0.1
$(package)_download_path=https://github.com/zcash/$(package)/archive/
$(package)_file_name=$(package)-$($(package)_git_commit).tar.gz
$(package)_download_file=$($(package)_git_commit).tar.gz
$(package)_sha256_hash=a5760a90d4a1045c8944204f29fa2a3cf2f800afee400f88bf89bbfe2cce1279
$(package)_git_commit=91348647a86201a9482ad4ad68398152dc3d635e
$(package)_dependencies=rust

$(package)_vendored_file_name=$(package)-$($(package)_git_commit)-vendored-sources.tar.gz
$(package)_vendored_sha256_hash=464af53d0843487fdebc734e703f5abd4f79486ca95a6b351d7fb643ccff0913
$(package)_extra_sources=$($(package)_vendored_file_name)

define $(package)_fetch_cmds
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_download_file),$($(package)_file_name),$($(package)_sha256_hash)) && \
(test -f $($(package)_source_dir)/$($(package)_vendored_file_name) || \
( mkdir -p $($(package)_download_dir) && echo Fetching $(package) vendored sources... && \
  ( $(build_DOWNLOAD) "$($(package)_download_dir)/$($(package)_vendored_file_name).temp" "$(PRIORITY_DOWNLOAD_PATH)/$($(package)_vendored_file_name)" || \
  ( tar xf $(rust_cached) -C $(host_prefix) && \
    $(host_prefix)/native/bin/cargo install --root $(host_prefix)/native cargo-vendor && \
    cd $($(package)_download_dir) && \
    tar --strip-components=1 -xf $($(package)_source) && \
    $(host_prefix)/native/bin/cargo vendor vendored-sources && \
    cd vendored-sources && \
    find . | sort | tar --no-recursion --mtime "2018-01-01 00:00:00" --owner=0 --group=0 -c -T - | gzip -9n > $($(package)_download_dir)/$($(package)_vendored_file_name).temp )) && \
  echo "$($(package)_vendored_sha256_hash)  $($(package)_download_dir)/$($(package)_vendored_file_name).temp" > $($(package)_download_dir)/.$($(package)_vendored_file_name).hash && \
  $(build_SHA256SUM) -c $($(package)_download_dir)/.$($(package)_vendored_file_name).hash && \
  mv $($(package)_download_dir)/$($(package)_vendored_file_name).temp $($(package)_source_dir)/$($(package)_vendored_file_name) && \
  rm -rf $($(package)_download_dir) ))
endef

define $(package)_extract_cmds
  echo "$($(package)_sha256_hash)  $($(package)_source)" > $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  echo "$($(package)_vendored_sha256_hash)  $($(package)_source_dir)/$($(package)_vendored_file_name)" >> $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  $(build_SHA256SUM) -c $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  mkdir vendored-sources && \
  tar --strip-components=1 -C vendored-sources -xf $($(package)_source_dir)/$($(package)_vendored_file_name) && \
  tar --strip-components=1 -xf $($(package)_source)
endef

define $(package)_preprocess_cmds
  mkdir .cargo && \
  echo "[source.vendored-sources]\ndirectory = \"$($(package)_extract_dir)/vendored-sources\"\n\n[source.crates-io]\nreplace-with = \"vendored-sources\"" > .cargo/config
endef

define $(package)_build_cmds
  cargo build --release
endef

define $(package)_stage_cmds
  mkdir $($(package)_staging_dir)$(host_prefix)/lib/ && \
  mkdir $($(package)_staging_dir)$(host_prefix)/include/ && \
  cp target/release/librustzcash.a $($(package)_staging_dir)$(host_prefix)/lib/ && \
  cp include/librustzcash.h $($(package)_staging_dir)$(host_prefix)/include/
endef
