FROM mcr.microsoft.com/devcontainers/base:ubuntu-24.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       # Core build tools
       build-essential \
       gcc \
       g++ \
       make \
       pkg-config \
       # Debugger and dynamic analysis
       gdb \
       valgrind \
       # Static analysis and formatting
       clang-format \
       clang-tidy \
       cppcheck \
       # Coverage reporting
       lcov \
       # Binary introspection (objdump, readelf, nm, addr2line)
       binutils \
       # Scripting and networking utilities
       python3 \
       curl \
       # Version control
       ca-certificates \
       git \
    && rm -rf /var/lib/apt/lists/*

# Ensure expected workspace location exists for VS Code remote container users.
RUN mkdir -p /workspaces
