
{ inputs ? {}, pkgs ? inputs.nixpkgs.legacyPackages.x86_64-linux }:
pkgs.stdenv.mkDerivation {

  pname = "mwm";
  version = "0.1";
  src = ./.;

  buildInputs = [

    pkgs.bear
    pkgs.coreutils
    pkgs.fontconfig 
    pkgs.freetype
    pkgs.gcc
    pkgs.harfbuzz
    pkgs.lua
    pkgs.libGL
    pkgs.libGLU
    pkgs.pkg-config
    pkgs.which
    pkgs.xorg.libX11
    pkgs.xorg.libXft
    pkgs.xorg.libXinerama

  ];

  buildPhase = ''
    export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:${pkgs.freetype}/lib/pkgconfig:${pkgs.fontconfig}/lib/pkgconfig:${pkgs.harfbuzz}/lib/pkgconfig:${pkgs.lua}/lib/pkgconfig:${pkgs.xorg.libX11}/lib/pkgconfig:${pkgs.xorg.libXft}/lib/pkgconfig:${pkgs.xorg.libXinerama}/lib/pkgconfig
    echo "PKG_CONFIG_PATH: $PKG_CONFIG_PATH"

    # List all available packages for debugging
    pkg-config --list-all

    g++ mwm.cpp -o mwm \
      $(pkg-config --cflags --libs harfbuzz) \
      $(pkg-config --cflags --libs freetype) \
      $(pkg-config --cflags --libs fontconfig) \
      $(pkg-config --cflags --libs lua) \
      $(pkg-config --cflags --libs xft) \
      $(pkg-config --cflags --libs xinerama) \
      -lX11 -lGL -lGLU -lm

    gcc mwm-cli.c -o mwm-cli
  '';

  installPhase = ''
    mkdir -p $out/bin
    cp mwm $out/bin/
    cp mwm-cli $out/bin/
  '';

  meta = with pkgs.lib; {
    description = "My window manager (dwm clone)";
    homepage = "";
    license = licenses.mit;
    maintainers = [ ];
  };
}
