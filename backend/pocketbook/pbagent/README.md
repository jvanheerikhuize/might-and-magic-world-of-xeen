# libpbagent (vendored)

pbtk's in-app agent: lets `pbtk` (CLI, GUI, and Claude Code through
`pbtk mcp`) take screenshots, inject taps and keys, stream the log, copy files
and restart the app over Wi-Fi while mmIV runs on the reader.

- Source: `device/libpbagent/` in the pbtk toolkit (pocketbook_lux_5);
  `VERSION` names the toolkit commit. Update with `tools/update-pbagent.sh`.
- Off unless the reader has an agent token (`pbtk token install`, or any
  `pbtk deploy` over USB writes `applications/pbagent.token`). Without one it
  logs one line and never touches Wi-Fi, so release builds behave as before.
- Hooked in by `platform-inkview.cpp`; not built for the host stub.
