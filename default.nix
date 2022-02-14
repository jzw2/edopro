{ pkgs ? import <nixpkgs> {} }:
with pkgs;
let edoIrr =
      (irrlicht.overrideAttrs (old: rec {

        version = "edo";

        src = fetchFromGitHub {
          owner = "edo9300";
          repo = "irrlicht1-8-4";
          rev = "6fecacf97e56aba50498eb1a15653366d2e29a59";
          # sha256 = "0000000000000000000000000000000000000000000000000000";
          sha256 = "sha256-jU54vH96QRRJOGRqEu/fiiHIOF3apnkd28xeuH+i1BQ=";
        };

        buildInputs = old.buildInputs ++ [
          libGL
          libxkbcommon
          wayland
          xorg.libX11
          xorg.libXxf86vm
        ];
      }));

    customFreetype = fetchzip {
      url = "http://downloads.sourceforge.net/freetype/freetype-2.6.5.tar.bz2";
      sha256 = "sha256-4HjWCJHG+51jNOrZ4zTua3BTLK+0iqVuMbuT9tPlBVA=";
    };
    type = "release";
in
stdenv.mkDerivation {
  name = "edopro";
  src = ./. ;
  # cmakeFlags = [
  #     "-DIRRLICHT_INCLUDE_DIR=${irrlicht}/include/irrlicht"
  #   ];

  CPLUS_INCLUDE_PATH = "${edoIrr}/include/irrlicht";
    buildInputs = with pkgs; [
            edoIrr
            fmt


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

            curl
            flac
            freetype
            libevent
            libgit2
            libvorbis
            nlohmann_json
            openal
            sqlite
            wayland
            xorg.libX11

            # For NO_IRR_WAYLAND_DYNAMIC_LOAD_
            libxkbcommon

            # For NO_IRR_DYNAMIC_OPENGL_
            libGL

            # For Lua
            readline

            # For fmt
            cmake

          ];


  VCPKG_ROOT= "vcpkg";
  # inherit irrlicht;

  buildPhase =

    ''
echo Extracting FreeType...
ln -s ${customFreetype}/builds freetype/builds
ln -s ${customFreetype}/include freetype/include
ln -s ${customFreetype}/src freetype/src
export LDFLAGS="$LDFLAGS -lwayland-client"
premake5 gmake2
make -Cbuild -j2 config=${type} ygoprodll
'';

  installPhase = ''
    mkdir -p $out/bin
    mv bin/${type}/ygoprodll $out/bin
  '';
}
