FROM devkitpro/devkitarm:20260610

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        imagemagick \
        mgba-sdl \
        python3 \
        scrot \
        xdotool \
        xvfb \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /work

