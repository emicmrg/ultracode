# DevForge - Multi-stage Build
# Stage 1: Build native libraries
FROM gcc:13 AS native-builder
WORKDIR /build
COPY lib/ /build/
RUN cmake -S . -B out -DCMAKE_BUILD_TYPE=Release && cmake --build out --parallel $(nproc)

# Stage 2: Build Go binaries
FROM golang:1.21-alpine AS go-builder
WORKDIR /src
COPY go.mod go.sum ./
RUN go mod download
COPY . .
RUN CGO_ENABLED=0 GOOS=linux go build -ldflags="-s -w" -o /out/server ./cmd/server
RUN CGO_ENABLED=0 GOOS=linux go build -ldflags="-s -w" -o /out/worker ./cmd/worker
RUN CGO_ENABLED=0 GOOS=linux go build -ldflags="-s -w" -o /out/migrate ./cmd/migrate

# Stage 3: Build frontend
FROM node:20-alpine AS frontend-builder
WORKDIR /app
COPY web/package.json web/package-lock.json ./
RUN npm ci --production
COPY web/ .
RUN npm run build

# Stage 4: Final runtime image
FROM alpine:3.19
RUN apk add --no-cache ca-certificates tzdata curl \
    && addgroup -S devforge && adduser -S devforge -G devforge

COPY --from=native-builder /build/out/libdevforge_*.so /usr/local/lib/
COPY --from=go-builder /out/server /usr/local/bin/devforge-server
COPY --from=go-builder /out/worker /usr/local/bin/devforge-worker
COPY --from=go-builder /out/migrate /usr/local/bin/devforge-migrate
COPY --from=frontend-builder /app/dist /usr/share/devforge/web
COPY config/default.yaml /etc/devforge/config.yaml
COPY scripts/migrate.sh /usr/local/bin/devforge-setup

RUN ldconfig /usr/local/lib && chmod +x /usr/local/bin/devforge-*

ENV DEVFORGE_CONFIG=/etc/devforge/config.yaml
EXPOSE 8080 9090
USER devforge

HEALTHCHECK --interval=30s --timeout=5s --retries=3 \
    CMD curl -f http://localhost:8080/health || exit 1

ENTRYPOINT ["devforge-server"]
