package=cppzmq
$(package)_version=4.2.2
$(package)_download_path=https://github.com/zeromq/$(package)/archive/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_download_file=v$($(package)_version).tar.gz
$(package)_sha256_hash=3ef50070ac5877c06c6bb25091028465020e181bbfd08f110294ed6bc419737d
$(package)_dependencies=zeromq

define $(package)_stage_cmds
  mkdir $($(package)_staging_dir)$(host_prefix)/include/ && \
  cp zmq.hpp $($(package)_staging_dir)$(host_prefix)/include/
endef
