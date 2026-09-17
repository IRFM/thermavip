# Continuous integration

Four entry points, three reusable workflows, one composite action. Nothing is
duplicated: the Qt / CMake / ctest mechanism is written once, in
`wf-build-test.yml`, and everything else calls it.

```
.github/
├── actions/setup-build-env/action.yml   system deps, Qt, Ninja, ccache/sccache
└── workflows/
    ├── wf-lint.yml         reusable   clang-format + cppcheck
    ├── wf-build-test.yml   reusable   configure, build, install, ctest, coverage
    ├── wf-heavy.yml        reusable   Valgrind, TSan, Windows leaks, clang-tidy, CodeQL, SDK
    │
    ├── branch.yml          push to any branch but main
    ├── ci.yml              pull request and push to main
    ├── nightly.yml         cron, 00:17 UTC
    └── release.yml         tag v*
```

## What runs when

| | branch | pull request | nightly | tag |
|---|---|---|---|---|
| clang-format, cppcheck, whole tree | **blocking** | **blocking** | **blocking** | **blocking** |
| build + ctest | 1 lane | 6 lanes, **blocking** | 6 lanes | 6 lanes, **blocking** |
| plugins (ffmpeg, python, hdf5) | no | no | yes | yes |
| warnings as errors | no | no | yes | no |
| coverage | artifact only | Codecov | Codecov | artifact only |
| AddressSanitizer | no | **blocking** | yes | **blocking** |
| Valgrind, ThreadSanitizer | no | no | report | **blocking** |
| Windows CRT leak check | no | no | report | **blocking** |
| SDK integration (`find_package`) | no | no | report | **blocking** |
| clang-tidy, CodeQL | no | no | report | report |
| packaging, release draft | no | no | no | yes |

The six build lanes are Linux, Windows and macOS x86_64, each with Qt 6.8.3 and
Qt 5.15.2. **macOS arm64 is not built**: the project does not target Apple
Silicon, so a lane for it would report on something nobody ships.

One rule explains what is blocking and what is not: **a blocking job is a job
people wait for.** AddressSanitizer costs about 2x and blocks every pull
request. Valgrind costs 10x and does not: a slow blocking job is a job people
learn to bypass. The slow tools become blocking exactly once, on a tag, where
waiting a few hours is acceptable.

Everything else blocks, including the two lint jobs over the whole tree and the
Qt 5.15 lanes. They will be red until the tree is formatted, the cppcheck
findings are triaged and Qt 5 is made to build — and that work is the point of
this pipeline, not an obstacle to it. Each job publishes what is needed to do
that work: clang-format uploads the complete patch, cppcheck the full list of
findings, every build lane its test results.

clang-tidy and CodeQL are the two exceptions that never block. A style finding
must not stop a release, and CodeQL's output is the security tab, not an exit
code.

## Where the tests live

```
src/Tests/                  18 QTest executables + 4 Python tests
├── CMakeLists.txt          one add_subdirectory per SDK library
├── setup_example.cmake     links the SDK, registers the CTest test, installs it
├── vip_test_main.h         VIP_TEST_MAIN, replaces QTEST_MAIN
├── Logging/  Format  Pimpl  SharedMemory
├── DataType/ Components  Histogram  NDArray  Resize  Serialization
├── Core/     Archive  CommandOptions  ProcessingObject  Python  Timer
├── Gui/      DragWidget  FileSystem  StandardWidgets
├── Annotation/ Json  Sql
└── Python/   four scripts run directly by the interpreter
```

The tree mirrors `src/`: one directory per SDK library, one leaf directory per
test executable, and the leaf holds nothing but a three-line `CMakeLists.txt`
and a `Test*.cpp`. Adding a test is adding a leaf and one
`add_subdirectory` line — never a change to the CI.

Everything the CI needs from a test comes from `src/Tests/setup_example.cmake`,
not from the workflows:

- `add_test()` and a 300 s timeout, so `ctest` finds it;
- `QT_QPA_PLATFORM=offscreen` and `QT_PLUGIN_PATH` as **test properties**. A
  CTest test property overrides what the job exports, so setting these in a
  workflow would be inert. This is why no lane runs `xvfb`: the tests need no
  display server by construction;
- on Windows, the SDK library directories prepended to `PATH` at test time
  rather than copied next to the executable — a copy is only refreshed when the
  test target relinks, so a library rebuilt on its own left the test running
  against a stale DLL;
- installation into `<prefix>/tests`.

`WITH_TESTS` (default `ON`) is the project switch; the CI passes it explicitly.
`ctest` is called with `--no-tests=error`, so a run that finds no test fails:
finding zero tests means `WITH_TESTS` or the registration is broken, which is
precisely what must not pass silently.

### What does *not* belong in `src/Tests`

`ci/sdk-consumer/` is a separate CMake project built **after**
`cmake --install`, against `CMAKE_PREFIX_PATH`. It is the only check of the
published interface: the install rules, the exported targets, the installed
headers, the generated `thermavipConfig.cmake` and `thermavip.pc`. It cannot
live in `src/Tests`, because a test there links the in-tree targets directly and
would pass on an install tree that is completely broken — which is the exact
failure it exists to catch.

## Suppression files

| file | read by | format |
|---|---|---|
| `cmake/sanitizer-suppressions.txt` | ASan, LSan | `kind:substring` |
| `cmake/valgrind-suppressions.txt` | Valgrind, via CTest | named blocks |

Two files because the two tools have incompatible syntaxes and neither reads the
other's. Both live under `cmake/` and are wired in
`cmake/thermavip_sanitizers.cmake`, so `ctest -T memcheck` behaves the same on a
developer machine and on a runner. The rules written at the top of each file —
one justification per entry, never an entry covering `src/` — are what keeps
them from becoming the place real leaks go to be forgotten.

## Pinned actions

Every external action is pinned to a commit SHA with the version in a trailing
comment. A tag can be moved by whoever owns it, and `jurplel/install-qt-action`'s
`v4` is a *branch*, not a tag. `.github/dependabot.yml` opens a reviewed pull
request for each new version, including for the composite action, which the
default `/` directory does not cover.

## Running the same checks locally

```sh
# build and test, as the CI does
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DWITH_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure --no-tests=error

# AddressSanitizer, same command as the CI
cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DTHERMAVIP_SANITIZER=address -DWITH_EXAMPLES=OFF
cmake --build build-asan --parallel
ctest --test-dir build-asan -T memcheck --output-on-failure

# Valgrind (Linux). The suppression file is picked up from cmake/ on its own.
cmake -S . -B build-vg -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DMEMORYCHECK_TYPE=Valgrind -DWITH_EXAMPLES=OFF
cmake --build build-vg --parallel
ctest --test-dir build-vg -T memcheck --output-on-failure

# the formatting check, which runs over the whole tree
git ls-files 'src/*.c' 'src/*.cpp' 'src/*.h' 'src/*.hpp' \
  | xargs clang-format -style=file -i
git diff          # empty means the CI will pass

# the cppcheck check, same invocation as the CI
cppcheck --enable=warning,performance,portability --inline-suppr --std=c++17 \
  --quiet --suppress=missingIncludeSystem --suppress=unusedFunction \
  -i 3rd_64 -i build src/
```

A cppcheck finding that is wrong is silenced next to the code it concerns:

```cpp
// cppcheck-suppress nullPointerRedundantCheck
```

`--inline-suppr` is what makes that work. There is deliberately no project-wide
cppcheck suppression list: a suppression that lives next to the code goes
through review, and a suppression in a shared file does not.
