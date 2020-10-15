ARG FROMBASEOS
ARG FROMBASEOS_BUILD_TAG=latest
FROM electriccoinco/zcashd-build-$FROMBASEOS$FROMBASEOS_BUILD_TAG

RUN useradd -ms /bin/bash -U debian
USER debian:debian
WORKDIR /home/debian
CMD ["sleep", "infinity"]