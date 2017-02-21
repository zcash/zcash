package=cppzmq
$(package)_version=4.2.1
$(package)_download_path=https://github.com/zeromq/$(package)/archive/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_download_file=v$($(package)_version).tar.gz
$(package)_sha256_hash=11c699001659336c7d46779f714f3e9d15d63343cd2ae7c1905e4bf58907cef9
$(package)_dependencies=zeromq

define $(package)_stage_cmds
  mkdir $($(package)_staging_dir)$(host_prefix)/include/ && \
  cp zmq.hpp $($(package)_staging_dir)$(host_prefix)/include/
endef
