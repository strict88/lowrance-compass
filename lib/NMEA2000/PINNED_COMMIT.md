# Vendored at a pinned commit

Source: https://github.com/ttlappalainen/NMEA2000.git
Commit: `5b7b9fc3ccc18e30ebfba92da6486cffc625159` (master, 2025-12-18)

Per `specs/001-calibration-guided-setup/research.md` §3, this project has no
tagged releases, so the exact commit is the version pin (constitution's "no
floating versions" rule). Vendored directly into `lib/` (a plain copy, `.git`
stripped) rather than referenced via `lib_deps` because PlatformIO's git
dependency fetcher does a shallow (`--depth=1`) fetch of the given ref, and
GitHub refuses to serve an arbitrary commit SHA that way (only branches/tags
resolve) -- confirmed by direct `git fetch` reproduction during
implementation. A full clone + checkout of this exact SHA works and is what
produced this directory.

To update the pin: `git clone https://github.com/ttlappalainen/NMEA2000.git`,
`git checkout <new-sha>`, copy the tree over this directory (minus `.git`),
update the commit above, and re-verify `pio run -e esp32s3` builds clean.
