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
| clang-format, cppcheck, whole tree | warning | warning | warning | warning |
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

Everything else blocks, the six build lanes and the Qt 5.15 rows included. They
will be red until Qt 5 is made to build, and that work is the point of this
pipeline, not an obstacle to it.

The two lint jobs are the exception: they report and never block. Neither
`src/.clang-format` nor cppcheck has ever been applied to this tree, so enforcing
either would stop every change until a backlog is cleared in one commit. Both
still print everything they found in their log, under a collapsed group, write a
breakdown to the job summary and attach the full report as an artifact — the
drift is visible on every run rather than forgotten. Turning one into a gate is
deleting its `::warning::` line in favour of an `::error::` and an `exit 1`.

clang-tidy and CodeQL never block either. A style finding must not stop a
release, and CodeQL's output is the security tab, not an exit code.

cppcheck runs with `--library=qt` and the SDK include paths. Without them two
thirds of its output was `unknownMacro` on `Q_OBJECT`, `Q_SLOTS` and the `VIP_*`
macros — the analyser reporting its own ignorance rather than anything about the
code. A finding that is wrong is silenced with a `// cppcheck-suppress <id>`
comment next to the code it concerns, never in a shared list: `--inline-suppr` is
enabled for that, and a suppression that lives next to the code goes through
review.

## Formatting rules

The formatting style is `src/.clang-format`, which has been in the repository
since the first commit: Mozilla with 8-column tabs, a 200-column limit,
`AccessModifierOffset: -8`, `NamespaceIndentation: All` and `SortIncludes: false`
(include order is hand-maintained here and reordering it would be a behaviour
change disguised as formatting).

There is deliberately no second `.clang-format` at the repository root.
clang-format searches upward from each file, so a root file would be shadowed by
`src/.clang-format` for every file the lint job checks, and the two would drift
apart without anything reporting it. If the rules ever need to cover code outside
`src/`, move that one file to the root rather than adding a second.

Measured on the first run of this pipeline: 330 of the 415 source files differ
from the style the project declares for itself, about 10 000 lines out of
245 000. The bulk of it is in the VTK and Python sources, which no default
configuration compiles — code nothing built is also code nothing formatted.

Applying it is a single commit, and it must be a commit of its own:

```sh
python -m pip install clang-format==18.1.8
git ls-files 'src/*.c' 'src/*.cpp' 'src/*.h' 'src/*.hpp' \
  | xargs clang-format -style=file -i
```

Pin the version. clang-format 19 and 20 do not produce the same output as 18,
and the runner uses 18: formatting with another version leaves the job reporting
differences that are not there.

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
