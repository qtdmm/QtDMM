#!/bin/sh
# Copyright (c) 2026 The QtDMM developers
# SPDX-License-Identifier: GPL-3.0-or-later
# Builds the signed Flatpak repository of QtDMM from release bundles.
#
#   tools/build-flatpak-repo.sh <stable.flatpak|-> <beta.flatpak|-> <out dir> <key id> <base url>
#
# <stable.flatpak> becomes the branch "stable", <beta.flatpak> the branch
# "beta" ("-" for none). <out dir> receives the OSTree repository (repo/),
# qtdmm.flatpakrepo, io.github.qtdmm.qtdmm.flatpakref, the beta's
# .flatpakref and the public key qtdmm.gpg. The key <key id> must be in the
# GnuPG home (GNUPGHOME) without a passphrase. <base url> is where <out dir>
# is served (https://qtdmm.github.io/QtDMM/repos/flatpak).
#
# The repository is made anew each time, from the newest release of each
# branch: Pages replaces the whole site, and flatpak update only compares the
# commit it has with the one offered.
set -eu

APP=io.github.qtdmm.qtdmm
stable=$1 beta=$2 out=$3 key=$4 url=$5
# the KDE runtime comes from Flathub; the application only from here
runtime_repo=https://dl.flathub.org/repo/flathub.flatpakrepo
icon=${url%/repos/flatpak}/qtdmm.svg

mkdir -p "$out"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
ostree init --mode=archive-z2 --repo="$out/repo"

# a bundle carries the branch it was built with (master): import it into a
# scratch repository and commit it again under the branch it is offered as
add() {
  bundle=$1 branch=$2
  [ "$bundle" = "-" ] && return 0
  rm -rf "$work/import"
  ostree init --mode=archive-z2 --repo="$work/import"
  flatpak build-import-bundle --no-update-summary "$work/import" "$bundle"
  src=$(ostree refs --repo="$work/import" | grep "^app/$APP/")
  arch=$(echo "$src" | cut -d/ -f3)
  flatpak build-commit-from --no-update-summary --gpg-sign="$key" \
    --src-repo="$work/import" --src-ref="$src" "$out/repo" "app/$APP/$arch/$branch"
  echo "$bundle -> app/$APP/$arch/$branch"
}
add "$stable" stable
add "$beta" beta
flatpak build-update-repo --gpg-sign="$key" --generate-static-deltas --prune "$out/repo"

gpg --export "$key" > "$out/qtdmm.gpg"
pubkey=$(base64 -w0 "$out/qtdmm.gpg")

cat > "$out/qtdmm.flatpakrepo" <<EOF
[Flatpak Repo]
Title=QtDMM
Url=$url/repo/
Homepage=https://qtdmm.de/
Comment=Digital multimeter readout and recorder
Icon=$icon
GPGKey=$pubkey
EOF

ref() {
  branch=$1 file=$2 title=$3
  cat > "$out/$file" <<EOF
[Flatpak Ref]
Name=$APP
Branch=$branch
Title=$title
Url=$url/repo/
SuggestRemoteName=qtdmm
Homepage=https://qtdmm.de/
Icon=$icon
RuntimeRepo=$runtime_repo
IsRuntime=false
GPGKey=$pubkey
EOF
}
[ "$stable" = "-" ] || ref stable "$APP.flatpakref" "QtDMM"
[ "$beta" = "-" ] || ref beta "$APP.beta.flatpakref" "QtDMM (release candidates)"
