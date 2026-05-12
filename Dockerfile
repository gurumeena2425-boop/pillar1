FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    g++ \
    make \
    cmake \
    git \
    python3 \
    python3-pip \
    tcpdump \
    tshark \
    wireshark-common \
    iproute2 \
    net-tools \
    iputils-ping \
    netcat-openbsd \
    libpcap-dev \
    vim \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

CMD ["/bin/bash"]