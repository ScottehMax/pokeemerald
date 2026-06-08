FROM debian:bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        bash \
        binutils-arm-none-eabi \
        build-essential \
        ca-certificates \
        gcc-arm-none-eabi \
        git \
        gosu \
        libnewlib-arm-none-eabi \
        libpng-dev \
        pkg-config \
        python3 \
    && rm -rf /var/lib/apt/lists/*

COPY docker/entrypoint.sh /usr/local/bin/pokeemerald-expansion-entrypoint
RUN chmod +x /usr/local/bin/pokeemerald-expansion-entrypoint

WORKDIR /workspace

ENTRYPOINT ["pokeemerald-expansion-entrypoint"]
CMD ["make"]
