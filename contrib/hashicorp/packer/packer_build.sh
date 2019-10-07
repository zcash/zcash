#! /usr/bin/env sh

DOCKERAPP_UID=$1        # This must be a UID, not a "name"!

# Get KEY and ID from AWS, e.g.:
# https://console.aws.amazon.com/iam/home?region=us-east-2#/security_credentials
AWS_ACCESS_KEY_ID=$2    # This is specific to your AWS login (see above)
AWS_SECRET_KEY=$3       # This is specific to your AWS login (see above)

# You need to create an ssh-keypair, e.g. like this
# https://github.com/zcash/zcash-gitian#decide-on-an-ssh-keypair-to-use-when-uploading-build-signatures-to-github
SSH_PUBKEY=$4           # Once your AMI is launced (e.g. as an EC2) you can use this keypair to ssh
IMAGE_SPECIFICATION=$5  # e.g. gitlabrunner/specification.json

PWD=$(pwd)

docker run --user=$DOCKERAPP_UID -v $PWD:$PWD -v $PWD/tmp:/tmp -w $PWD \
    registry.gitlab.com/zingo-labs/zcash/packer:0.1 build \
                                                    -var $AWS_ACCESS_KEY_ID \
                                                    -var $AWS_SECRET_KEY \
                                                    -var $SSH_PUBKEY \
                                                    $IMAGE_SPECIFICATION > buildlog.txt
