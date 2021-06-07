ARG ARCHLINUX_TAG
FROM archlinux:$ARCHLINUX_TAG

RUN pacman -Syyu --noconfirm \
    && pacman -S --noconfirm \
      base-devel \
      git \
      python3 \
      python-pip \
      wget

RUN wget -O /usr/bin/dumb-init https://github.com/Yelp/dumb-init/releases/download/v1.2.2/dumb-init_1.2.2_amd64
RUN chmod +x /usr/bin/dumb-init
RUN python -m pip install virtualenv
# AUR for libtinfo5 Source: https://dev.to/cloudx/testing-our-package-build-in-the-docker-world-34p0
RUN useradd builduser -m \
  && passwd -d builduser \
  && cd /home/builduser \
  && git clone "https://aur.archlinux.org/ncurses5-compat-libs.git" ncurses5-compat-libs \
  && chown builduser -R ncurses5-compat-libs \
  && (printf 'builduser ALL=(ALL) ALL\n' | tee -a /etc/sudoers) \
  && sudo -u builduser bash -c 'gpg --keyserver pool.sks-keyservers.net --recv-keys 702353E0F7E48EDB' \
  && sudo -u builduser bash -c 'cd ~/ncurses5-compat-libs && makepkg -si --noconfirm'