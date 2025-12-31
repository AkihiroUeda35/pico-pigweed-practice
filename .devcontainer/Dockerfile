FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive

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
    libusb-1.0-0-dev \
    libhidapi-dev \
    libgpiod-dev \
    libftdi1-dev \
    libjim-dev \
    libtool \
    autoconf \
    automake \
    pkg-config 

# The 'ubuntu' has sudo privileges.
RUN echo "ubuntu ALL=(root) NOPASSWD:ALL" > /etc/sudoers.d/ubuntu \
    && chmod 0440 /etc/sudoers.d/ubuntu

# Set up Zephyr SDK v0.17.0
WORKDIR /opt
RUN wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.17.0/zephyr-sdk-0.17.0_linux-x86_64.tar.xz \
    && tar xvf zephyr-sdk-0.17.0_linux-x86_64.tar.xz \
    && rm zephyr-sdk-0.17.0_linux-x86_64.tar.xz \
    && ./zephyr-sdk-0.17.0/setup.sh -t arm-zephyr-eabi -h -c
ENV ZEPHYR_SDK_INSTALL_DIR=/opt/zephyr-sdk-0.17.0

RUN git clone https://github.com/raspberrypi/openocd.git \
    && cd openocd && ./bootstrap \
    && ./configure --prefix=$ZEPHYR_SDK_INSTALL_DIR/sysroots/x86_64-pokysdk-linux/usr/ \
    && make -j 8  && make install \
    && cd .. && rm -rf openocd

RUN npm install n -g && n stable && npm install -g @google/gemini-cli

RUN PROTOC_VERSION="29.3" && \
    PROTOC_ZIP="protoc-${PROTOC_VERSION}-linux-x86_64.zip" && \
    curl -LO "https://github.com/protocolbuffers/protobuf/releases/download/v${PROTOC_VERSION}/${PROTOC_ZIP}" && \
    unzip -o "${PROTOC_ZIP}" -d /usr/local bin/protoc && \
    unzip -o "${PROTOC_ZIP}" -d /usr/local 'include/*' && \
    rm -f "${PROTOC_ZIP}" && \
    chmod +x /usr/local/bin/protoc

RUN BIN="/usr/local/bin" && \
    VERSION="1.62.1" && \
    curl -sSL \
    "https://github.com/bufbuild/buf/releases/download/v${VERSION}/buf-$(uname -s)-$(uname -m)" \
    -o "${BIN}/buf" && \
    chmod +x "${BIN}/buf"

COPY --from=ghcr.io/astral-sh/uv:latest /uv /uvx /bin/

USER ubuntu
WORKDIR /home/ubuntu

# Install West and ensure it's in the PATH
RUN pip3 install --user --break-system-packages west
ENV ZEPHYR_SDK_INSTALL_DIR=/opt/zephyr-sdk-0.17.0