FROM node:20-alpine@sha256:eabac870db94f7342d6c33560d6613f188bbcf4bbe1f4eb47d5e2a08e1a37722

ENV USER chall
ENV PORT 3000

RUN adduser -D -g "" $USER

COPY --chown=root:root ./deploy /app


WORKDIR /app
RUN apk add --no-cache netcat-openbsd
RUN npm install


CMD until nc -z -w1 db 3306;    \
        do sleep 1;             \
    done;                       \
    exec npm start
USER $USER
EXPOSE $PORT