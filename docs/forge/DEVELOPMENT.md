# Forge developer guide

## Get the development branch

```sh
git clone --branch forge/foundation https://github.com/DCDominguez/CHOMPI.git
cd CHOMPI
```

For an existing clone, first preserve any local work, then fetch and switch to
`forge/foundation`. The active work is in [draft PR #1](https://github.com/DCDominguez/CHOMPI/pull/1).
This guide does not require merging the branch or flashing a unit.

## Requirements

| Task | Requirements |
| --- | --- |
| Host JSON validation / encoding | Python 3.10+; standard library only |
| Host MIDI I/O | Python plus `host/requirements.txt`; CHOMPI or a bidirectional MIDI interface |
| Webapp / cloud AI | Python standard library, browser, OpenAI or Gemini API key and structured-output model ID |
| Optional CLI AI | A configured Ollama server and an already-installed model |
| Native software tests / renderer | GNU Make, a C++14 compiler, pthreads and Python 3.10+ |
| Firmware build | GNU Make and GNU Arm Embedded 10.3-2021.10 |

Native tests and the ARM build have been exercised in Linux. The upstream
[quickstart](../../firmware/README.md) covers macOS toolchain setup. Windows users
can run the Python host locally; a Linux environment such as WSL is a reasonable
build route, but that route has not been tested for this candidate. Keep MIDI
device access on the native host unless you have verified forwarding.

The [firmware guide](../../firmware/chompi-forge/README.md) gives the compiler
download and checksum. Keep that version: upstream documents compatibility
concerns with other compilers. No separate SDK clone is required; libraries are
vendored in this repository.

## Validate software

Run from the repository root:

```sh
make -C firmware/chompi-forge test
make -C firmware/chompi-forge sanitize
```

`test` builds the DSP/queue, protocol, synth and v3 suites and the offline harness, then runs
Python integration tests. `make -C firmware/chompi-forge browser-test` drives the
real webapp in Chromium (Playwright) against `tests/sim_device.py`, a stateful
stand-in that routes SysEx through the same C++ runtime; screenshots land in
`build/browser/`. It needs `pip install playwright` plus a Chromium install
matching that Playwright version. Where Chromium is preinstalled (e.g.
`/opt/pw-browsers/chromium-1194` in agent sandboxes), install the matching
release (`pip install playwright==1.56.0`) instead of `playwright install`. It
proves browser behaviour and layout, not hardware, audio or live providers. `sanitize` runs the C++ suites with ASan/UBSan.
If a container prevents LeakSanitizer from inspecting `/proc`, rerun with
`ASAN_OPTIONS=detect_leaks=0` and explicitly record that leak checking was
disabled. A pass in that mode is not a leak-test result.

For host-only commands and model configuration, follow the
[host guide](../../firmware/chompi-forge/host/README.md).

## Build firmware

```sh
make -C firmware/chompi-forge firmware GCC_PATH=/absolute/toolchain/path/bin
```

`GCC_PATH` is the directory containing `arm-none-eabi-gcc` and
`arm-none-eabi-g++`, not the executable itself. If the correct toolchain is
already on PATH, omit the argument.

The target rebuilds libDaisy into `firmware/chompi-forge/build/libdaisy/` and
links the application using WAVE's BOOT_SRAM layout. Outputs are
`firmware/chompi-forge/src/build/FORGE.bin`, `.elf`, `.hex`, and `.map`.
Keep the map for memory allocation review; it does not measure runtime CPU or
worst-case stack headroom. Forge's Makefile rejects flashing targets.

If the pinned Arm archive cannot be downloaded (blocked in the agent sandboxes
on 2026-10-02/03), the checksum-verified xPack build of the same GCC
release, `xpack-arm-none-eabi-gcc-10.3.1-2.3`, from
github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack works with the same
`GCC_PATH` usage. It is not byte-identical; the packager records the real
compiler from `FORGE.elf` and flags `built_with_pinned_compiler`.

## Create a test bundle

Commit the tested source first and choose an output path outside the repository:

```sh
cd firmware/chompi-forge
python3 host/package_candidate.py /absolute/output/Forge_0.4_Test_Candidate.zip
unzip Forge_0.4_Test_Candidate.zip && python3 Forge-0.4-test-*/verify_bundle.py
```

Run `make test` and the firmware build before packaging. The script requires a
clean checkout and existing firmware/harness outputs. It includes firmware,
host tools, presets, simulated reference audio, documentation and licenses.
Its manifest records source commit/tree, compiler version and file SHA-256s.
The script does not itself rerun all tests or certify physical readiness.
Existing ZIPs are not overwritten; choose a new output path for a new candidate.

`--source-commit` is only for a known equivalent remote commit when content was
uploaded through a connector. Verify that commit's tree matches the local tree;
do not use the override to label unverified source.

Generated binaries, build directories, WAV simulations and ZIPs are not tracked
in git. Software-only source changes still need a new binary and bundle if the
candidate should contain them. Documentation-only changes leave an earlier
bundle's recorded source identity intact.

## Troubleshooting

| Symptom | Check / next action |
| --- | --- |
| ARM compiler not found | Verify `GCC_PATH` points to the compiler's `bin` directory |
| Compiler crashes even on trivial C | Verify archive checksum; this execution environment required extracting the compiler to `/tmp` instead of the workspace mount |
| Archive extraction reports unexpected EOF | Download the archive again and verify its checksum before use |
| Host cannot import MIDI dependencies | Install the pinned requirements with the same Python interpreter used to run the host |
| MIDI port not found | Run `ports` and copy exact input/output names; never select a synth implicitly |
| No acknowledgement | Confirm firmware/ports and both MIDI directions; a patch may have applied, so query status before retrying |
| AI request fails or emits invalid JSON | Verify the configured model/service; malformed output must remain rejected, not silently repaired or sent |
| Sound differs from simulation | Capture actual targets and logs; simulation does not validate codec levels, board routing or CPU timing |

## Change discipline

Keep allocations, storage, parsing, network/model calls and MIDI transmission
out of the Forge audio core/callback. Preserve single-producer/single-consumer
ownership. Add tests for changed behavior and failure paths, rather than merely
mirroring code. Update the protocol and both host/device implementations together
when changing wire semantics. Record results and limitations in the handoff.

Follow DC's one-session hardware-test plan. New software work can be validated
offline; physical claims remain pending until recorded in the consolidated
session. Do not change the bootloader as a routine Forge development step.

Instrument development starts with [CONTINUE.md](CONTINUE.md). `make test` runs
four native suites (core, protocol, synth, v3) and the Python tests, including
v1/v2/v3 round trips through the C++ codec. When changing the v3 layout, edit
`V3Fields` (core/protocol.h) and `V3_FIELDS` (host/forge_host.py) together.
Any v1/v2 DSP change must keep their output bit-exact or be called out as a
deliberate behaviour change. The package generator includes all schemas, ten
presets, web assets and simulated renders. Browser and live API acceptance are
separate from mocked tests.
