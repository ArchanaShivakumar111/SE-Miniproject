ARG BASE_IMAGE=ubuntu:22.04

FROM ${BASE_IMAGE} AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential \
        cppcheck \
        valgrind && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY Makefile ./
COPY include/ ./include/
COPY src/ ./src/
COPY tests/ ./tests/

RUN make clean && \
    make && \
    make lint && \
    make test && \
    make valgrind && \
    make asan && \
    make check-globals

FROM ${BASE_IMAGE} AS runtime

WORKDIR /app

COPY --from=builder /app/build/simple_shell /usr/local/bin/simple_shell

ENTRYPOINT ["/usr/local/bin/simple_shell"]
