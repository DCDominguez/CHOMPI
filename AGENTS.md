# Forge development guidance

Read `docs/forge/CONTINUE.md` first, then HANDOFF, PROJECT and PROTOCOL. CONTINUE
is the live checkpoint while the instrument milestone is being built. Update it
at every meaningful checkpoint and before handing off. Record exact checks,
failures and unverified behavior; never present work in progress as completed.

User scope: AI-programmable playable instrument, not effects-only. Current work:
four-voice synth + existing delay, keybed/MIDI, versioned module patches, AI webapp.
One consolidated hardware test; do not request per-feature flashing. Keep upstream
firmware separate. No main-branch merge, hardware flash or real-key API calls
without the corresponding authorization/access. No secrets in source or logs.

Run native tests for audio/protocol changes, sanitizers for DSP changes, and ARM
build for firmware changes. Keep host/device wire format and tests consistent.
Backward-compatible v1 delay presets must remain usable. Bound audio work and
keep mutable DSP state in the audio callback; no dynamic allocation or blocking
I/O there. Update documentation and preserve a runnable, documented checkpoint.

Forge v2 (parked): storage streaming, `docs/forge/STORAGE_STREAMING.md`. When v1
changes SDRAM use, the sampler read path, loader/recorder/looper, USB file
replacement, FatFS/SD config or code headroom, update that study's figures.
