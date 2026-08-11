FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libpqxx-dev \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*

RUN git config --global http.sslVerify false

WORKDIR /src
COPY . .

RUN cmake -S server -B build -DCMAKE_BUILD_TYPE=Release -DREDIS_TESTS=ON -DNATS_TESTS=ON -DPROCESS_SPLIT_TESTS=ON \
    && cmake --build build --config Release -j 4

CMD ["./build/KungFuChessServerTests"]
