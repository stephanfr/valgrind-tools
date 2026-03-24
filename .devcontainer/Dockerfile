FROM mcr.microsoft.com/devcontainers/base:ubuntu-24.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       build-essential \
       gcc \
       g++ \
       make \
       gdb \
       valgrind \
       ca-certificates \
       git \
       pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Ensure expected workspace location exists for VS Code remote container users.
RUN mkdir -p /workspaces
