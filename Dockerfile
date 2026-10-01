FROM debian:bookworm-slim AS build

RUN apt-get update \
    && apt-get install -y --no-install-recommends g++ \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

COPY include ./include
COPY src/http_server.cpp ./src/http_server.cpp

RUN g++ -std=c++17 -O2 -pthread -Iinclude \
    -o /merqnet-engine src/http_server.cpp

FROM debian:bookworm-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /merqnet-engine /usr/local/bin/merqnet-engine

EXPOSE 8080

USER nobody

CMD ["merqnet-engine"]
