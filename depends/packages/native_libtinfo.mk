package=native_tinfo
$(package)_version=5.6.0
$(package)_download_path_linux=http://ftp.debian.org/debian/pool/main/n/ncurses/
$(package)_download_file_linux=libtinfo5_6.0+20161126-1+deb9u2_amd64.deb
$(package)_file_name_linux=libtinfo5_6.0+20161126-1+deb9u2_amd64.deb
$(package)_sha256_hash_linux=1d249a3193568b5ef785ad8993b9ba6d6fdca0eb359204c2355532b82d25e9f5

define $(package)_extract_cmds
	mkdir -p $($(package)_extract_dir) && \
	echo "$($(package)_sha256_hash)  $($(package)_source)" > $($(package)_extract_dir)/.$($(package)_file_name).hash && \
	$(build_SHA256SUM) -c $($(package)_extract_dir)/.$($(package)_file_name).hash && \
	mkdir -p libtinfo5 && \
	ar x --output libtinfo5 $($(package)_source_dir)/$($(package)_file_name) && \
	cd libtinfo5 && \
	tar xf data.tar.xz
endef

define $(package)_stage_cmds
	pwd && \
	mkdir -p $($(package)_staging_prefix_dir)/lib && \
	cp libtinfo5/lib/x86_64-linux-gnu/libtinfo.so.5.9 $($(package)_staging_prefix_dir)/lib/libtinfo.so.5
endef
