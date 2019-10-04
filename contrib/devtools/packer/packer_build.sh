#! /usr/bin/env sh

docker run -v `pwd`:`pwd` -w `pwd` registry.gitlab.com/zingo-labs/zcash/packer:1.0 build \
    -var 'aws_access_key_id={YOURAWSKEYID}' \
    -var 'aws_secret_key={YOURAWSSECRET}' \
    -var 'ssh_pubkey={YOURAWSPUBKEYFILE}' $@ | cut -d':' -f2 | xargs > built_image_id.txt
