# meshoptimizer 0.22 (vendored)

`meshoptimizer.h`, `allocator.cpp`, `simplifier.cpp` and `vcacheoptimizer.cpp`
are copied unmodified from meshoptimizer's `src/` directory; `LICENSE.md` is its
MIT licence. The renderer uses the simplifier to build the reduced copy of a
soldier's full mesh (cloak, equipment and all) drawn when the soldier is small on
screen, and the vertex cache optimiser to reorder that copy's triangles.

Regenerate from the release tarball
<https://github.com/zeux/meshoptimizer/archive/refs/tags/v0.22.tar.gz>
(SHA-256 `e296cf0685b6421f84bd8a44d0a3cca82a219500f11c793dfbb6087ec86bb1a3`):

```sh
tar xzf meshoptimizer-0.22.tar.gz
cd meshoptimizer-0.22
cp src/meshoptimizer.h src/allocator.cpp src/simplifier.cpp src/vcacheoptimizer.cpp \
   LICENSE.md <repo>/third_party/meshoptimizer/
```

It is vendored rather than fetched at configure time so a build never depends
on GitHub serving the tarball.
