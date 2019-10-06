#! /usr/bin/env sh

docker run -v `pwd`:`pwd` -w `pwd` \
    registry.gitlab.com/zingo-labs/zcash/packer:1.0 $1 \
                                                    -var $2 \
                                                    -var $3 \
                                                    -var $4 \
                                                    $5 > buildlog.txt
