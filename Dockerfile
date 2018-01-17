FROM debian:jessie

ENV ZCASH_CONF /home/zcash/.zcash/zcash.conf
ENV ZCASH_DATA /home/zcash/.zcash
ENV GOSU_VERSION 1.10

RUN set -e && \
  # Installing required debian packages
  apt-get update -q && \
  apt-get install --no-install-recommends -q -y apt-transport-https ca-certificates wget && \
  # Installing gosu which will be used in entrypoint script to change permissions for data folder to zcash:zcash
	wget -O /usr/local/bin/gosu "https://github.com/tianon/gosu/releases/download/$GOSU_VERSION/gosu-$(dpkg --print-architecture)" && \
	wget -O /usr/local/bin/gosu.asc "https://github.com/tianon/gosu/releases/download/$GOSU_VERSION/gosu-$(dpkg --print-architecture).asc" && \
	export GNUPGHOME="$(mktemp -d)" && \
	gpg --keyserver ha.pool.sks-keyservers.net --recv-keys B42F6819007F00F88E364FD4036A9C25BF357DD4 && \
	gpg --batch --verify /usr/local/bin/gosu.asc /usr/local/bin/gosu && \
	rm -rf "$GNUPGHOME" /usr/local/bin/gosu.asc && \
	chmod +x /usr/local/bin/gosu && \
	gosu nobody true && \
  # Installing zcash
  wget -qO - https://apt.z.cash/zcash.asc | apt-key add - && \
  echo "deb [arch=amd64] https://apt.z.cash/ jessie main" | tee /etc/apt/sources.list.d/zcash.list && \
  apt-get update -q && \
  apt-get install zcash -q -y && \
  zcash-fetch-params && \
  groupadd -r zcash && \
  useradd -r -m -g  zcash zcash && \
  mv /root/.zcash-params /home/zcash/ && \
  mkdir -p /home/zcash/.zcash/ && \
  chown -R zcash:zcash /home/zcash && \
  # Cleanup
  apt-get purge -q -y apt-transport-https ca-certificates wget && \
  apt-get clean -q -y && \
  apt-get autoclean -q -y && \
  apt-get autoremove -q -y && \
  rm -rf "/var/lib/apt/lists/*" "/var/lib/apt/lists/partial/*" "/tmp/*" "/var/tmp/*"

VOLUME /home/zcash/.zcash

COPY docker-entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]

EXPOSE 8232 8233 18232 18233

CMD [ "zcashd" ]
