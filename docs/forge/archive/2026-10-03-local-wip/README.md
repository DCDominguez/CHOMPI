# Archived local-only commits (2026-10-03)

Two commits from an earlier cloud session that were never pushed. They existed only in
that container's history; they are kept here as `git format-patch` files so nothing is
lost when sessions move (2026-10-09). **Superseded, do not apply:** the pushed history
redid this work.

| Patch | Original | Superseded by |
| --- | --- | --- |
| 0001 Hardware test bridge: audio capture/analysis, plan runner, smoke plan | `8ea052e`, parent `7e8fd19` (on the branch) | `50cdc4b` plug-and-play bridge and later bridge commits; its `tests/hardware/smoke.json` (20-step v0.5 plan) became `host/auto_checks.json` (34 steps) |
| 0002 WIP (local only): audio bridge docs, dev packaging, panel release safety | `7f587fd`, parent `8ea052e` | the same files exist on the branch in later, tested versions |

To inspect: `git apply --stat 0001-*.patch`, or `git am` onto a scratch branch at `7e8fd19`.
