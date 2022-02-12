{ pkgs ? import <nixpkgs> {} }:
with pkgs;
stdenv.mkDerivation {
  name = "edopro";
  src = ./. ;
  buildInputs = with pkgs; [
    upx

    git curl p7zip
    gcc
    unzip
    # tar
    readline
    freetype
    irrlicht
    libGLU
    lua5_4
    # (premake5.overrideAttrs (old: rec {
    #     version = "5.0.0-beta1";

    #     src = fetchFromGitHub {
    #       owner = "premake";
    #       repo = "premake-core";
    #       rev = "v${version}";
    #       sha256 = "sha256-8D9032JLAog1PvvsBODVlqFoBy86z9z2L4I9GlTbPw4=";
    #     };
    #   }))
  ];

  buildPhase = ''
echo "setting a bunch of dumb variables"
export DEPENDENCIES_BASE_URL=https://github.com/edo9300/edopro-vcpkg-cache/releases/download/20220210
export BUILD_CONFIG=release


export DEPLOY_BRANCH="travis-linux"
export TRAVIS_OS_NAME=linux


export VCPKG_ROOT=$PWD/vcpkg
export VCPKG_CACHE_7Z_URL=$DEPENDENCIES_BASE_URL/installed_x64-linux.7z


echo "patching shebangs"
patchShebangs ./travis/*

bash ./travis/install-premake5.sh


bash ./travis/dependencies.sh



bash ./travis/build.sh
bash ./travis/predeploy.sh


bash ./travis/predeploy.sh
  '';

  installPhase = ''
    mkdir -p $out/bin
    mv mytest.txt $out/bin
  '';
}
