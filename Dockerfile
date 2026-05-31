FROM debian:bookworm-slim

ARG AGBCC_REPO=https://github.com/pret/agbcc.git
ARG AGBCC_REF=da598c1d918402c42c0c0d7128ba14567f3175e9

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        bash \
        binutils-arm-none-eabi \
        build-essential \
        ca-certificates \
        git \
        gosu \
        libpng-dev \
        perl \
        pkg-config \
    && rm -rf /var/lib/apt/lists/*

RUN git clone --depth 1 "${AGBCC_REPO}" /opt/agbcc \
    && cd /opt/agbcc \
    && git fetch --depth 1 origin "${AGBCC_REF}" \
    && git checkout FETCH_HEAD \
    && ./build.sh

COPY docker/entrypoint.sh /usr/local/bin/pokeemerald-entrypoint
RUN chmod +x /usr/local/bin/pokeemerald-entrypoint

WORKDIR /workspace

ENTRYPOINT ["pokeemerald-entrypoint"]
CMD ["make"]
