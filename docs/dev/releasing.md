# Versions and releases

## Version scheme

QtDMM uses calendar versioning ([CalVer](https://calver.org/)) in the form
**YY.N**: the two-digit year, then a running number for the releases of that
year. A version number thus says how current a build is:

| Version | Meaning |
|---|---|
| `26.1` | first release of 2026 (`26.2` the second, `27.1` the first of 2027) |
| `26.1.1` | bugfix release of 26.1 |
| `26.1-rc1` | release candidate 1 of 26.1 (also `-beta1`, `-alpha1`) |
| `26.2-dev.14+g1234abc` | development build: 14 commits after 26.1, heading for 26.2 |
| `26.1-rc1.3+g1234abc` | 3 commits after 26.1-rc1, before 26.1 |
| `26.4-dev.7+g1234abc` | master, 7 commits after the tag `26.4-dev`, while 26.3 is finished on its release branch |

There is no fixed release calendar; N simply counts the releases of the year.

The version comes from **git tags**; the one thing to bump by hand is
`qtdmm.spec` (step 2).
`cmake/git_version.cmake` runs `git describe` on the nearest tag of the form
`YY.N[.P][-rcK]`, and once more on the nearest `YY.N-dev` tag; the one heading
for the higher version counts (see [Release branch](#release-branch)). The old
tags `0.9.x` and `1.0.0-alpha.1` do not match and are ignored. The same version appears in different forms:

| Where | Form | Example |
|---|---|---|
| UI, `--version`, SCPI `*IDN?` | as above | `26.1-rc1` |
| RPM / DEB | `~` sorts a pre-release before the release | `26.1~rc1`, `26.2~dev.14.g1234abc` |
| Arch (`PKGBUILD`) | VCS form | `26.1rc1.r3.g1234abc` |
| macOS bundle, Windows file version | numbers only | `26.1.0` |
| Windows download names | `+` replaced by `-` | `QtDMM-26.1-rc1-windows-x64.exe` |

Builds without `.git` still know their version:

- The CPack source tarball carries a `.tarball-version` file.
- GitHub's source downloads get it through `export-subst` in `.archive-version`.
- So does the source tarball of a release (`QtDMM-26.2.tar.gz`, from 26.2 on):
  `git archive` of the tag, the same content as GitHub's download, but with a
  checksum that stays, which distributions keep in their package recipes.
  The Release workflow stops when its `.archive-version` does not say the
  tag.

A clone needs its tags: `git fetch --tags`. The CI checkouts use `fetch-depth: 0`. `ctest -R version_scheme` checks the mapping from tag to version.

## Making a release

1. **CHANGELOG.md**: give the top section its version and date,
   `## v26.3 — <title> (2026-11-22)` instead of `## Unreleased — 26.3 (not released yet)`.
2. **AppStream** (`assets/appimage/qtdmm.appdata.xml`): add the release at
   the top of `<releases>`. A release candidate gets `type="development"`
   and a `~` instead of the `-`: AppStream sorts `26.1-rc1` after `26.1`, and
   the `appstream_metadata` test fails on the order.

   ```xml
   <releases>
     <release version="26.1" date="2026-10-12"/>
     <release version="26.1~rc1" date="2026-10-01" type="development"/>
   </releases>
   ```

   In `qtdmm.spec`, set `Version:` to the new version (`26.2`) at its first
   release candidate - Copr builds `master` with it. A release candidate has
   `Release: %autorelease -p -e rcN` and `%global tag %{version}-rcN` (the tag
   of the source tarball `Source0` downloads); the final release drops
   `-p -e rcN` and sets `%global tag %{version}` again.
3. Merge that as a normal PR.
4. Before tagging, try the macOS release path once: *Actions → macOS test
   build → Run workflow* on the release branch (`release/26.3`) with
   **release** ticked; a push to the branch builds the `.dmg` only for one
   architecture. It builds the
   universal `.dmg` exactly as the tag will; a failure shows up here instead
   of in the release.
5. Tag the merge commit on the release branch and push the tag:

   ```bash
   git fetch upstream
   git tag -a 26.1-rc1 -m "QtDMM 26.1-rc1" upstream/release/26.1
   git push git@github.com:qtdmm/QtDMM.git 26.1-rc1
   ```

   The tag must reach qtdmm/QtDMM itself: a tag on a fork starts no
   Release workflow there. A clone whose `upstream` has its push URL
   switched off (`DISABLE`, against pushing a branch there by mistake)
   pushes the tag to the repository's URL directly, as above; a clone
   that may push to `upstream` can write `git push upstream 26.1-rc1`.

6. The **Release** workflow (`.github/workflows/release.yml`) builds the
   Linux AppImage and Flatpak bundle (`QtDMM-<tag>-x86_64.flatpak`,
   `.github/workflows/flatpak.yml`), the Windows installer and ZIP and the
   universal macOS `.dmg`. It then creates a **draft** release with them and
   the source tarball `QtDMM-<tag>.tar.gz`, marked as a pre-release when the
   tag has a suffix. If only the macOS or the Flatpak job fails, the draft
   is still made, without the `.dmg` or the `.flatpak`.
   If the release was made on GitHub's release page instead (which creates
   the tag), the workflow adds the files to it and leaves it as it is. Its
   builds take about 10 minutes (the universal macOS build is the slowest),
   and until then the release has no binaries.
   Mark a release candidate as a pre-release there by hand.
7. Check the draft on GitHub: download and start the builds, then write the
   notes (from CHANGELOG.md). Publish it. The macOS `.dmg` is only checked by
   CI (bundle contents, architectures, signature, minimum macOS, a start
   with `--version`); test it on a Mac when one is available.
8. Publishing the release runs the **GitHub Pages** workflow
   (`.github/workflows/pages.yml`): it rebuilds the signed Flatpak repository
   at <https://qtdmm.github.io/QtDMM/repos/> from the bundles of the newest
   release (branch `stable`) and of a newer release candidate (branch
   `beta`), with `tools/build-flatpak-repo.sh`. It signs with the secret
   `REPO_GPG_KEY` (an armored secret key without passphrase); without it the
   page says the repository is not online yet. A release published on the
   release page before its files were there gets its Flatpak bundle into the
   repository too: the Release workflow starts Pages again after the upload.
   A release runs on its tag, so the environment `github-pages` (*Settings →
   Environments*) needs a rule of ref type **Tag**, `[0-9]*`, next to the
   branch `master`; without it the deploy job fails with "Tag ... is not
   allowed to deploy to github-pages" (set up for 26.3-rc1).
9. Tell the website: the download page links the new release.

## Release branch

From the first release candidate on, the release is finished on a branch of
its own, and `master` is open for the next one:

1. **At rc1:** branch `release/26.3` from `master` (not `26.3`: a branch and
   a tag of the same name make git's refs ambiguous). Tag `26.3-rc1` on it.
2. **On master:** tag the first commit that is only there `26.4-dev`
   (`git tag -a 26.4-dev -m "26.4 development"`), so master builds say
   `26.4-dev.N`. Not the branch point itself: that one is on the release
   branch too. Then the CHANGELOG gets its section for 26.4.
3. **Fixes for 26.3:** pull requests against `release/26.3`. Merge the
   branch into `master` after each one (or a few), so the fixes are there
   too; merging rather than cherry-picking leaves none behind.
4. **Further candidates and the release:** `26.3-rc2`, `26.3` are tagged on
   the release branch, with the steps above.
5. **After the release:** the branch stays for bugfix releases (`26.3.1`,
   tagged the same way on it).

The CI builds pushes and pull requests of `release/**` as of `master`; Pages
and the Flatpak repository follow `master` and the tags.

`git describe` takes the nearest tag. Once the release branch is merged into
master, its `26.3-rc2` can be nearer than `26.4-dev`, so the version script
looks for both and takes the one heading for the higher version: 26.4 on
master, `26.3-rc2` on the release branch (where `26.4-dev` is not), and after
`26.3` both say 26.4. `ctest -R version_scheme` checks that choice too.
