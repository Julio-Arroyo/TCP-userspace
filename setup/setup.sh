#!/usr/bin/env bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

sudo apt-get update

sudo DEBIAN_FRONTEND=noninteractive apt-get install -y \
  build-essential vim cppcheck \
  emacs tree tmux git gdb valgrind python3-dev libffi-dev libssl-dev \
  clang-format iperf3 tshark iproute2 iputils-ping net-tools tcpdump \
  cmake g++-13

sudo DEBIAN_FRONTEND=noninteractive apt-get install -y \
  python3 python3-pip python-tk libpython3.10-dev libcairo2 \
  libcairo2-dev pre-commit

pip3 install --upgrade pip
pip3 install -r $SCRIPT_DIR/../requirements.txt

