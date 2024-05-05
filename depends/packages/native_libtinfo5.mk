package=native_libtinfo5
$(package)_version=6.2

# We only enable this if build_os is linux.
$(package)_download_path=http://ftp.debian.org/debian/pool/main/n/ncurses/
$(package)_download_file=libtinfo5_$($(package)_version)+20201114-2+deb11u2_amd64.deb
$(package)_file_name=libtinfo5-$($(package)_version).deb
$(package)_sha256_hash=69e131ce3f790a892ca1b0ae3bfad8659daa2051495397eee1b627d9783a6797

define $(package)_extract_cmds
	mkdir -p $($(package)_extract_dir) && \
	echo "$($(package)_sha256_hash)  $($(package)_source)" > $($(package)_extract_dir)/.$($(package)_file_name).hash && \
	$(build_SHA256SUM) -c $($(package)_extract_dir)/.$($(package)_file_name).hash && \
	mkdir -p libtinfo5 && \
	cd libtinfo5 && \
	ar x $($(package)_source_dir)/$($(package)_file_name) && \
	tar xf data.tar.xz
endef

define $(package)_stage_cmds
	pwd && \
	mkdir -p $($(package)_staging_prefix_dir)/lib && \
	cp libtinfo5/lib/x86_64-linux-gnu/libtinfo.so.5.9 $($(package)_staging_prefix_dir)/lib/libtinfo.so.5
endef
