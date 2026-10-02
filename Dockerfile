# ---------- Etape 1 : compilation ----------
FROM debian:bookworm-slim AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
        g++ cmake ninja-build ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY backend/ backend/

RUN cmake -S backend -B /build -G Ninja -DCMAKE_BUILD_TYPE=Release \
 && cmake --build /build -j"$(nproc)" \
 && /build/gestionnaire_tests

# ---------- Etape 2 : image d'execution ----------
FROM debian:bookworm-slim

# curl est installe uniquement pour le HEALTHCHECK ci-dessous.
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates curl \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --create-home --uid 10001 app

WORKDIR /app
COPY --from=build /build/gestionnaire_server /app/gestionnaire_server
COPY backend/migrations/ /app/migrations/
COPY frontend/ /app/frontend/

RUN mkdir -p /app/data && chown -R app:app /app
USER app

ENV APP_HOST=0.0.0.0 \
    APP_PORT=8080 \
    APP_DB_PATH=/app/data/app.db \
    APP_MIGRATIONS_DIR=/app/migrations \
    APP_FRONTEND_DIR=/app/frontend \
    APP_LOG_LEVEL=INFO

EXPOSE 8080
VOLUME ["/app/data"]

HEALTHCHECK --interval=30s --timeout=3s --start-period=5s \
    CMD curl -fsS http://127.0.0.1:8080/api/health || exit 1

CMD ["/app/gestionnaire_server"]
