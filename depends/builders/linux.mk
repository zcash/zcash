build_linux_SHA256SUM = sha256sum
build_linux_DOWNLOAD = curl --location --fail --connect-timeout $(DOWNLOAD_CONNECT_TIMEOUT) --retry $(DOWNLOAD_RETRIES) --retry-delay 0 --retry-max-time 40 --fail -o
