This PR provides developers with the ability to produce Virtual Machine Images with minimal effort.

Included features:

* minimal prerequisites:
     - a development environment that includes Docker
     - sufficient authority for resource allocation (e.g. AWS key and key ID)
* maximal auditability; where possible content-based hashes are included in the code
     - TODO: Describe the FROM hash, and signature
     - The "source_ami" is from this list: https://wiki.debian.org/Cloud/AmazonEC2Image/Buster
     - TODO: Describe the packer commit hash
* portability:
     - Produced Docker images are small enough to transfer conveniently
* optionality:
     - Build it yourself, or use a standard image
* composeability:
    - The tool is intended to be a component of a more complete CI solution
    - The included AWS config, is only one example of use

This utility produces computer Machine Images with minimal configuration by the User.

