{ pkgs ? import <nixpkgs> {} }:
with pkgs;
let cacheFile =
  let
DEPENDENCIES_BASE_URL = "https://github.com/edo9300/edopro-vcpkg-cache/releases/download/20220210";
VCPKG_CACHE_7Z_URL = "${DEPENDENCIES_BASE_URL}/installed_x64-linux.7z";
    in
  fetchurl {
  url = VCPKG_CACHE_7Z_URL;
  # sha256 = "1111111111111111111111111111111111111111111111111111";
  sha256 = "sha256-eKGDD3qQobX4pjDL5Oxta+Tw4h2w0tS0ckgAS40152M=";
};
    in
stdenv.mkDerivation {
  name = "edopro";
  src = ./. ;
  # cmakeFlags = [
  #     "-DIRRLICHT_INCLUDE_DIR=${irrlicht}/include/irrlicht"
  #   ];

  # CPLUS_INCLUDE_PATH = "${irrlicht}/include/irrlicht";
  # CPLUS_INCLUDE_PATH = "${irrlicht}/include/irrlicht";
  buildInputs = with pkgs; [
    upx

    git curl p7zip
    # gcc
    clang
    unzip
    # tar
    readline
    freetype
    # irrlicht
    mesa
    libGLU
    lua5_4
    xorg.libX11
    xorg.libX11.dev
    (premake5.overrideAttrs (old: rec {
        version = "5.0.0-alpha15";

        src = fetchFromGitHub {
          owner = "premake";
          repo = "premake-core";
          rev = "v${version}";
          # sha256 = "0000000000000000000000000000000000000000000000000000";
          # 12
          # sha256 = "sha256-7mN7zhJ0++YO2xq1E0APskzwWaMCvEWstCT9dk3KcMA=";
          #
          # 14
          # sha256 = "sha256-qwIE9qLE0hLNsZPhDrS1+BzwE3nZi8wcQ1AGS88SlrI=";
          #
          # 15
          sha256 = "sha256-8Pdhqwek8zWLcahMgV+fdbsU3tA9oOnefvn6vvBhvDE=";
        };
      }))
  ];


VCPKG_ROOT= "vcpkg";
# inherit irrlicht;
inherit cacheFile;

buildPhase =

  ''
pwd
echo "root directory is $VCPKG_ROOT"
mkdir -p "$VCPKG_ROOT"
cd "$VCPKG_ROOT"
pwd
7z x ${cacheFile}
echo "finished 7zipping"
export CPLUS_INCLUDE_PATH=$CPLUS_INCLUDE_PATH:"$PWD/installed/x64-linux/include/irrlicht"
echo "$CPLUS_INCLUDE_PATH"
ls
cd ..
echo "Finished extraction"
echo "my files are currently"
ls
premake5 gmake2 --vcpkg-root=$VCPKG_ROOT
make -Cbuild -j2 config=debug ygoprodll
'';

  installPhase = ''
    mkdir -p $out/bin
    mv mytest.txt $out/bin
  '';
}
