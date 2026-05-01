# syntax=docker/dockerfile:1

ARG UBUNTU_VERSION=24.04

FROM ubuntu:${UBUNTU_VERSION} AS build

ARG DEBIAN_FRONTEND=noninteractive
ARG CMAKE_BUILD_TYPE=Release

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        cmake \
        g++ \
        make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -S . -B /tmp/signalroute-build \
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} \
        -DSR_BUILD_TESTS=OFF \
    && cmake --build /tmp/signalroute-build -j2 --target signalroute

FROM ubuntu:${UBUNTU_VERSION} AS runtime

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        libstdc++6 \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --create-home --home-dir /var/lib/signalroute --shell /usr/sbin/nologin signalroute \
    && mkdir -p /etc/signalroute \
    && chown -R signalroute:signalroute /etc/signalroute /var/lib/signalroute

COPY --from=build /tmp/signalroute-build/signalroute /usr/local/bin/signalroute
COPY --from=build /src/config/signalroute.toml /etc/signalroute/signalroute.toml

USER signalroute
WORKDIR /var/lib/signalroute

EXPOSE 9090 9091 9100 9101

ENTRYPOINT ["/usr/local/bin/signalroute"]
CMD ["--config=/etc/signalroute/signalroute.toml"]
