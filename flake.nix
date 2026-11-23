# Copyright (c) 2026 The QtDMM developers
# SPDX-License-Identifier: GPL-3.0-or-later

{
  description = "QtDMM - a digital multimeter front-end";

  # nix run github:qtdmm/QtDMM       start QtDMM
  # nix build                        build it into ./result
  # nix develop                      a shell with everything to build and test
  #
  # Linux only: on macOS and Windows the build fetches hidapi itself, which
  # the Nix sandbox does not allow.

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAll = f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});

      # The source in the Nix store has no .git: the version comes from
      # .tarball-version (cmake/git_version.cmake) - no tag, the commit date
      # and hash. A build shows "<year>.1-dev.<count>+g<hash>".
      date = self.lastModifiedDate or "19700101000000";
      day = "${builtins.substring 0 4 date}.${builtins.substring 4 2 date}.${builtins.substring 6 2 date}";
      rev = self.shortRev or self.dirtyShortRev or "unknown";
      count = toString (self.revCount or 0);

      qtdmm = pkgs: pkgs.stdenv.mkDerivation {
        pname = "qtdmm";
        version = "unstable-${builtins.replaceStrings [ "." ] [ "-" ] day}";
        src = self;

        nativeBuildInputs = with pkgs; [ cmake ninja pkg-config qt6.wrapQtAppsHook qt6.qttools ];
        buildInputs = with pkgs; [
          hidapi
          qt6.qtbase
          qt6.qtconnectivity
          qt6.qtmultimedia
          qt6.qtserialport
          qt6.qtsvg
          qt6.qtwayland
        ];

        cmakeFlags = [ "-DBUILD_TESTING=OFF" "-DQTDMM_UPDATE_CHECK=OFF" ];

        preConfigure = ''
          printf '%s\n' "" "${day}" "${count}" "${rev}" > .tarball-version
        '';

        meta = {
          description = "Graphical front-end for digital multimeters";
          homepage = "https://qtdmm.de";
          license = with pkgs.lib.licenses; [ gpl3Plus lgpl3Plus ];   # QtDMM; the Breeze and Oxygen symbols
          mainProgram = "qtdmm";
          platforms = pkgs.lib.platforms.linux;
        };
      };
    in
    {
      packages = forAll (pkgs: {
        default = qtdmm pkgs;
      });

      devShells = forAll (pkgs: {
        default = pkgs.mkShell {
          inputsFrom = [ (qtdmm pkgs) ];
          packages = with pkgs; [ ccache python3 ];
        };
      });
    };
}
