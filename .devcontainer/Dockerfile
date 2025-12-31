FROM ubuntu:24.04

# Avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install system dependencies for Zephyr and Pigweed
RUN apt-get update && apt-get install -y --no-install-recommends \
    git \
    cmake \
    ninja-build \
    gperf \
    ccache \
    dfu-util \
    device-tree-compiler \
    wget \
    python3-dev \
    python3-pip \
    python3-setuptools \
    python3-tk \
    python3-wheel \
    python3-venv \
    xz-utils \
    file \
    make \
    gcc \
    g++ \
    libsdl2-dev \
    libmagic1 \
    unzip \
    sudo \
    curl \
    nodejs \
    npm \
    openocd \
    && rm -rf /var/lib/apt/lists/*

# The 'ubuntu' user already exists in the 24.04 base image.  it has sudo privileges.
RUN echo "ubuntu ALL=(root) NOPASSWD:ALL" > /etc/sudoers.d/ubuntu \
    && chmod 0440 /etc/sudoers.d/ubuntu

# Install protoc
RUN PROTOC_VERSION="29.3" && \
    PROTOC_ZIP="protoc-${PROTOC_VERSION}-linux-x86_64.zip" && \
    curl -LO "https://github.com/protocolbuffers/protobuf/releases/download/v${PROTOC_VERSION}/${PROTOC_ZIP}" && \
    unzip -o "${PROTOC_ZIP}" -d /usr/local bin/protoc && \
    unzip -o "${PROTOC_ZIP}" -d /usr/local 'include/*' && \
    rm -f "${PROTOC_ZIP}" && \
    chmod +x /usr/local/bin/protoc

# Set up Zephyr SDK v0.17.0
RUN mkdir -p /opt/zephyr-sdk \
    && cd /opt/zephyr-sdk \
    && wget -q https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.17.0/zephyr-sdk-0.17.0_linux-x86_64.tar.xz \
    && tar xf zephyr-sdk-0.17.0_linux-x86_64.tar.xz \
    && rm zephyr-sdk-0.17.0_linux-x86_64.tar.xz \
    && cd zephyr-sdk-0.17.0 \
    && ./setup.sh -t all -h -c

# Install Gemini CLI globally
RUN npm install n -g && n stable && npm install -g @google/gemini-cli

# Install buf
RUN BIN="/usr/local/bin" && \
    VERSION="1.62.1" && \
    curl -sSL \
    "https://github.com/bufbuild/buf/releases/download/v${VERSION}/buf-$(uname -s)-$(uname -m)" \
    -o "${BIN}/buf" && \
    chmod +x "${BIN}/buf"

# Install uv
COPY --from=ghcr.io/astral-sh/uv:latest /uv /uvx /bin/

# Switch to the 'ubuntu' user
USER ubuntu
WORKDIR /home/ubuntu

# Install West and ensure it's in the PATH
RUN pip3 install --user --break-system-packages west
ENV PATH="/home/ubuntu/.local/bin:${PATH}"