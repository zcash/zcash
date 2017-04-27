FROM debian:jessie

ENV ZCASH_CONF=/home/zcash/.zcash/zcash.conf
ENV ZCASH_DATA /home/zcash/.zcash

RUN set -e && \
  apt-get update -q && \
  apt-get install -q -y apt-transport-https wget && \
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
  apt-get purge -q -y apt-transport-https wget && \
  apt-get clean -q -y && \
  apt-get autoclean -q -y && \
  apt-get autoremove -q -y && \
  rm -rf "/var/lib/apt/lists/*" "/var/lib/apt/lists/partial/*" "/tmp/*" "/var/tmp/*"

VOLUME /home/zcash/.zcash

COPY docker-entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]

USER zcash

EXPOSE 8232 8233 18232 18233

CMD [ "zcashd" ]
