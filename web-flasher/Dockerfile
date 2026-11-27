FROM node:22-alpine

WORKDIR /app

COPY server.mjs ./
COPY public ./public

ENV NODE_ENV=production \
    PORT=8080 \
    GITHUB_REPO=Redalin/esp32FlightClock \
    DEFAULT_BRANCH=main \
    CACHE_TTL_SECONDS=300 \
    APP_TITLE="ESP32-C3 Flight Clock"

EXPOSE 8080

USER node

HEALTHCHECK --interval=30s --timeout=5s --start-period=5s --retries=3 \
  CMD wget -qO- http://127.0.0.1:8080/health || exit 1

CMD ["node", "server.mjs"]
