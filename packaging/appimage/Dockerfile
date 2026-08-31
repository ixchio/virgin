FROM ubuntu:26.04

ENV DEBIAN_FRONTEND=noninteractive \
    LANG=C.UTF-8 \
    LC_ALL=C.UTF-8 \
    APPIMAGE_EXTRACT_AND_RUN=1

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates curl file patchelf desktop-file-utils \
        cmake g++ ninja-build pkg-config libsqlite3-dev libxkbcommon-dev \
        qt6-base-dev libqt6sql6-sqlite qt6-webengine-dev \
    && rm -rf /var/lib/apt/lists/* \
    && curl --fail --location --retry 3 \
        https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage \
        --output /usr/local/bin/linuxdeploy \
    && curl --fail --location --retry 3 \
        https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage \
        --output /usr/local/bin/linuxdeploy-plugin-qt \
    && chmod 0755 /usr/local/bin/linuxdeploy /usr/local/bin/linuxdeploy-plugin-qt

WORKDIR /src

CMD ["bash", "packaging/appimage/build-inside-container.sh"]
