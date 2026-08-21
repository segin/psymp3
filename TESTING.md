# PsyMP3 Testing Guide

This document covers running the test suite, the test harness, and the conventions tests are expected to follow — fixtures, skipping, timeouts, and the setup that standalone test binaries must do for themselves.

## Quick Start

**Run all tests:**
```bash
make check
```

This runs the automake test suite (`tests/`) and then re-runs everything through the unified test harness. It works in both regular and `--enable-final` (unity) trees: the unity configuration builds the per-file objects tests link against through a check-only shadow library in `src/Makefile.am`, so `make check` never links stale leftovers.

On a machine without a D-Bus session bus, wrap it:
```bash
dbus-run-session -- make check
```

## Fixtures and Skipping

### The exit-77 SKIP convention

A test that cannot run because of its **environment** — missing test data, no D-Bus session bus, a feature disabled at configure time, a required external tool absent — must **exit 77**, the automake SKIP status. Both harnesses honor it:

- The automake runner counts it as `SKIP` and stays green.
- The custom test harness reports it as `SKIPPED` (yellow in console output, `<skipped/>` in JUnit XML) and excludes it from the failure verdict.

Never return 0 for "couldn't run" (hides coverage loss) and never return 1 (fails the suite for something that is not a defect).

### FLAC fixtures

Real-file FLAC tests resolve their input through `tests/flac_test_data_utils.h`:

- `FLACTestDataUtils::findAvailableTestFile()` returns the first existing candidate, preferring the generated fixture (`data/fixture.flac`, then `tests/data/fixture.flac` — both prefixes are listed because the harness may run with its working directory at either `tests/` or the project root), with a few legacy personal-library filenames as local fallbacks.
- `FLACTestDataUtils::validateTestDataAvailable("<test name>")` prints the candidate list and **exits 77** when nothing is present. Call it at the top of `main()` before touching any file.
- Never hardcode absolute paths into anyone's music library.

Generate the fixture locally the same way CI does (10 seconds of stereo noise):

```bash
mkdir -p tests/data
head -c 1764000 /dev/urandom > /tmp/noise.raw
flac --force-raw-format --channels=2 --bps=16 --sample-rate=44100 \
     --sign=signed --endian=little -o tests/data/fixture.flac /tmp/noise.raw
```

Ogg/Opus real-file tests have no synthesizable fixture (encoding one needs tools CI lacks); they SKIP when their sample file is absent.

### Synthetic media data

Hand-built container data must be **structurally valid** — the demuxers verify checksums and reject fiction:

- **FLAC**: build STREAMINFO with the shared `FLACTestDataUtils::appendStreamInfoBody()` serializer (RFC 9639 bit packing; per-test hand-rolled builders drifted and truncated fields). Synthetic frames need a real header CRC-8 (poly 0x07) and footer CRC-16 (poly 0x8005) — the demuxer validates both before returning a chunk.
- **Ogg**: pages need real CRCs — build them and run `ogg_page_checksum_set()` over the correct span (26-byte fixed header + count byte + segment table); libogg silently discards mismatching pages. Vorbis streams need the full ID/comment/setup header handshake before the demuxer reports a parsed container.
- Do not assert that seeks or reads succeed into regions the synthetic stream does not actually contain (e.g. a one-frame file advertising a megasample duration); assert graceful failure and an in-range position instead.

## Writing Tests: Required Setup

Standalone test binaries do not get the setup the player performs at startup. Do it yourself:

- **Codec/demuxer registration**: call `registerAllCodecs()` / `registerAllDemuxers()` (from `codecs/CodecRegistration.h`) in `main()`. Without it every registry lookup fails.
- **Use the production factory**: codecs are created through `AudioCodecFactory::createCodec()` — the same path `DemuxedStream` uses. `CodecRegistry` is a parallel registry that not every codec registers into.
- **Complete `StreamInfo`**: set `codec_type = "audio"` (several `canDecode()` implementations reject anything else) and, for native FLAC, `codec_tag = 0x43614C66` — a "flac" stream with tag 0 is routed to the Ogg FLAC passthrough codec, not the native decoder.
- **SDL audio**: initialize once per process and default to the dummy driver so headless runs work: `setenv("SDL_AUDIO_DRIVER", "dummy", 0)` before `SDL_Init(SDL_INIT_AUDIO)`. SDL3's `SDL_Init` returns `bool` — do not test it with `< 0`. Never call full `SDL_Quit()` between subtests.
- **Debug output**: `Debug::log(...)` is silent unless `Debug::init(logfile, {channels...})` ran. A test whose only failure reporting goes through `Debug::log` fails invisibly — print failures to stdout/stderr.

### MPRIS / D-Bus tests

- **Real libdbus objects only.** Never `reinterpret_cast` C++ mock objects to `DBusMessage*` / `DBusConnection*` / `Player*` — libdbus types are opaque C structs and the first real call on a fake one crashes. Build genuine messages with `dbus_message_new_method_call()` (stamp a serial with `dbus_message_set_serial()` if a reply will reference it) and use a real private bus connection.
- **Null player is the supported test configuration.** `MPRISManager(nullptr)` / `MethodHandler(nullptr, pm)` initialize fully; commands are answered with error replies — which still count as `DBUS_HANDLER_RESULT_HANDLED`.
- **Pump the service.** The manager only answers while `MPRISManager::processEvents()` runs. Single-threaded tests must interleave pumping with client calls (or use a pump thread); a blocking `send_with_reply_and_block` against your own unpumped service times out.
- **Target the acquired name.** When another player owns `org.mpris.MediaPlayer2.psymp3`, the manager registers the `.instance<pid>` fallback — query `MPRISManager::getServiceName()` instead of assuming the well-known name (or you will test the user's live player).
- **Discriminate transport errors.** `dbus_connection_send_with_reply_and_block(..., nullptr)` swallows ERROR replies and returns null — indistinguishable from "no such method"/timeout. Pass a `DBusError` and treat only `NoReply`/`Timeout`/`Disconnected`/`ServiceUnknown` as transport failures; an error reply proves the service is alive.
- **Skip without a bus**: probe `dbus_bus_get_private(DBUS_BUS_SESSION, ...)` in `main()` and exit 77 on failure. CI wraps `make check` in `dbus-run-session`.
- Tests that restart their own `dbus-daemon` must open connections from the **current** `DBUS_SESSION_BUS_ADDRESS` (`dbus_connection_open_private` + `dbus_bus_register`); libdbus caches the session address globally on first use and would keep dialing the dead pre-restart socket.

### Timeouts and flakiness

- The harness default per-test timeout is **120s** (long-running stress suites legitimately exceed the old 30s). Override per test with a source annotation (`// @test-timeout: 5000`, milliseconds) or globally with `./test-harness -t <seconds>`.
- Don't gate assertions on wall-clock performance thresholds or lock-contention ratios — CI machines run loaded; assert sanity (ratio within [0,1], operations completed) instead of magic numbers.
- If a mock is configured with a random failure rate, don't assert single operations succeed — retry setup operations, and let the stress phase own the randomness.

## Test Harness Options

The unified harness lives at `tests/test-harness` (built by `make check` when configured with `--enable-test-harness`, the default).

**Basic usage:**
```bash
cd tests && ./test-harness                    # Run all tests
cd tests && ./test-harness -v                # Verbose output
cd tests && ./test-harness -l                # List available tests
cd tests && ./test-harness -q                # Quiet mode (summary only)
```

**Filtering and control:**
```bash
cd tests && ./test-harness -f "*rect*"       # Run tests matching a pattern
cd tests && ./test-harness -s                # Stop on first failure (skips don't stop the run)
cd tests && ./test-harness -t 60             # 60-second timeout per test (default 120)
cd tests && ./test-harness -d /path/to/tests # Specify test directory
```

**Parallel execution:**
```bash
cd tests && ./test-harness -p                # Run tests in parallel
cd tests && ./test-harness -p -j 8           # Use 8 parallel processes
```

**Output formats:**
```bash
cd tests && ./test-harness -o xml > results.xml    # JUnit-style XML (skips emit <skipped/>)
cd tests && ./test-harness -o json > results.json  # JSON output
cd tests && ./test-harness -o console              # Console output (default)
```

**Performance analysis:**
```bash
cd tests && ./test-harness --track-performance     # Enable performance tracking
cd tests && ./test-harness --show-performance      # Show performance report
cd tests && ./test-harness --analyze-trends        # Analyze performance trends
```

## Individual Test Execution

```bash
cd tests
make test_rect_containment
./test_rect_containment
```

Run individual tests from the `tests/` directory so relative fixture paths (`data/fixture.flac`) resolve.

## Sanitizers and Property Tests

- Configure with `--enable-asan`, `--enable-ubsan`, or `--enable-tsan` for sanitizer builds.
- Property-based tests use RapidCheck: `./configure --enable-rapidcheck && make check`.

## Continuous Integration

CI (`.github/workflows/c-cpp.yml`) builds in a Debian trixie container (SDL3), generates the FLAC fixture with the `flac` CLI, and runs `dbus-run-session -- make check`. For report artifacts:

```bash
make check && cd tests && ./test-harness -o xml > test-results.xml
```

## Troubleshooting

1. **A test "fails" with exit 77**: it skipped — provision the fixture or bus it asked for; the suites treat it as SKIP, not failure.
2. **Individual test debugging**: run the failing binary directly with the working directory at `tests/`.
3. **`make check` fails to build**: ensure `autoconf-archive` is installed for git builds, dependencies are present, and `make` completed first; check `config.log`.
4. **Everything MPRIS fails at once**: you have no session bus — use `dbus-run-session -- make check`.

## Contributing Tests

1. Place test files in `tests/`, named `test_<component>.cpp`.
2. Add build rules to `tests/Makefile.am` (`check_PROGRAMS` membership puts the test in both the automake run and the harness).
3. Follow the conventions above: register codecs, complete `StreamInfo`, fixture-or-skip with exit 77, structurally valid synthetic data, real libdbus objects, no wall-clock assertions.
4. Ensure the test passes both individually and under `make check`.
