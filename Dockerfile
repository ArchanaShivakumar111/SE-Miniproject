FROM ubuntu:24.04

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    build-essential \
    cppcheck && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY Makefile ./
COPY include/ ./include/
COPY src/ ./src/
COPY tests/ ./tests/

RUN make clean && make && make test && make lint

CMD ["./build/simple_shell"]
