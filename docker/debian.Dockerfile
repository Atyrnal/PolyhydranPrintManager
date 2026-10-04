# syntax=docker/dockerfile:1
# Build from the repo root:
#   docker build -f docker/debian.Dockerfile --target export \
#       --output type=local,dest=build/debian-release .
FROM debian:trixie AS build

ENV DEBIAN_FRONTEND=noninteractive \
    LC_ALL=C.UTF-8

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build git ca-certificates \
    qt6-base-dev qt6-base-private-dev qt6-declarative-dev \
    qt6-httpserver-dev qt6-websockets-dev qt6-serialport-dev \
    qt6-svg-dev libxkbcommon-dev libcurl4-openssl-dev libzip-dev \
    libkf6windowsystem-dev qml6-module-qtquick qml6-module-qtquick-controls \
    qml6-module-qtquick-layouts qml6-module-qtquick-window \
    qml6-module-qtquick-templates qml6-module-qtquick-dialogs \
    qml6-module-qtqml-workerscript qml6-module-qtqml-models \
    qml6-module-qt-labs-folderlistmodel qml6-module-qtquick-shapes \
    qml6-module-qtquick-effects \
 && rm -rf /var/lib/apt/lists/*

# Qt MQTT isn't packaged by Debian. The tag must match Debian's Qt (6.8.2).
RUN git clone --branch v6.8.2 --depth 1 https://github.com/qt/qtmqtt.git /tmp/qtmqtt \
 && cmake -S /tmp/qtmqtt -B /tmp/qtmqtt/build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release -DQT_BUILD_EXAMPLES=OFF -DQT_BUILD_TESTS=OFF \
 && cmake --build /tmp/qtmqtt/build \
 && cmake --install /tmp/qtmqtt/build

WORKDIR /src
COPY . .
# Same preset layout as the host: build tree ends up in /src/build/debian-release.
# The config tool is off by default (BUILD_CONFIG_TOOL=OFF), so it isn't built.
RUN cmake --preset debian-release \
 && cmake --build --preset debian-release

# Collect outputs; cp -L resolves the Qt MQTT symlink into a real file
RUN mkdir -p /out/lib \
 && cp build/debian-release/print-manager/appPolyhydranPrintManager /out/ \
 && cp -L /usr/lib/x86_64-linux-gnu/libQt6Mqtt.so.6 /out/lib/

# Export-only stage: contains nothing but the files we want on the host
FROM scratch AS export
COPY --from=build /out/ /
