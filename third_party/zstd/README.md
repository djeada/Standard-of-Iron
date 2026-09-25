# Zstandard 1.5.7 (vendored)

`zstd.c` is Zstandard's official single-file amalgamation and `zstd.h` its
public header (with `zstd_errors.h`, which it includes). All are unmodified, and `LICENSE` is zstd's BSD licence. The
game uses it to decompress the baked creature caches; the asset baker also
uses it to compress them (see `animation/bpat/asset_compression.h`).

Regenerate from the release tarball
<https://github.com/facebook/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.gz>
(SHA-256 `eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3`):

```sh
tar xzf zstd-1.5.7.tar.gz
cd zstd-1.5.7/build/single_file_libs && ./create_single_file_library.sh
cp zstd.c ../../lib/zstd.h ../../lib/zstd_errors.h ../../LICENSE <repo>/third_party/zstd/
```

It is vendored rather than fetched at configure time so a build never depends
on GitHub serving the tarball. A transient HTTP 500 on that download once
failed a release preflight.
