

{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = [
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
    pkgs.xorg.xorgserver
  ];
}
