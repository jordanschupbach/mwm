# default:
#     nix develop . --command bash -c "mkdir -p build && cmake -S . -B build && cmake --build build -- -j$(nproc)"

default:
    gcc mwm.c -o mwm \
    $(pkg-config --cflags --libs harfbuzz) \
    $(pkg-config --cflags --libs freetype2) \
    $(pkg-config --cflags --libs fontconfig) \
    $(pkg-config --cflags --libs lua) \
    $(pkg-config --cflags --libs xft) \
    $(pkg-config --cflags --libs xinerama) \
    -lX11 -lGL -lGLU -lm
    gcc mwm-cli.c -o mwm-cli

# Test target
try: default
    DISPLAY=:0 Xephyr :1 -screen 1024x768 & export DISPLAY=:1; sleep 0.1; ./mwm  #nix run .

dev:
    nix-shell
