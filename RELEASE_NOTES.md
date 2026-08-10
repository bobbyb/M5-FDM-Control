### M5 FDM Control V0.1.0

First release of this fork. A native Apple Silicon (arm64) build of eufyMake Studio for
AnkerMake M5 / M5C printers, based on upstream eufyMake Studio v1.5.26.

M5 FDM Control is a slimmed-down fork of M5 FDM Studio v0.1.0. The feature set below is
inherited from it unchanged; this fork only removes files the macOS arm64 build never uses
and renames the app. It installs alongside M5 FDM Studio, but shares its data directory,
so presets and login carry over.

The stock release ships a closed-source, x86_64-only network plugin (`libAnkerNet.dylib`)
that handles login, device discovery, telemetry, and printing. Because the app cannot
load it natively on Apple Silicon, everything runs under Rosetta. This fork replaces that
plugin with a from-scratch native implementation, so the app runs natively on arm64.

Presets, configuration, and profiles carry over unchanged — the profile directory is
deliberately unchanged from eufyMake Studio, so an existing setup keeps working.

This is an unofficial community fork. It is not affiliated with, endorsed by, or
supported by Anker or eufyMake.

### What's new:

1. **Native Apple Silicon build**
No Rosetta. The proprietary network plugin is replaced by a native arm64 implementation
built from scratch, covering the account, device, telemetry, and printing paths.

2. **Account sign-in with a persistent session**
Sign-in runs through the normal eufyMake web login. The session is cached locally, so
relaunching keeps you signed in instead of asking every time.

3. **Printer list and live monitoring**
Your printers load automatically. Nozzle and bed temperatures, print state, progress,
elapsed and remaining time, filament usage, and layer counts all update live over MQTT.

4. **Printer controls**
Nozzle and bed temperature targets, Z-offset, and filament extrude/retract.

5. **Send to printer — local network and remote**
Sliced jobs upload over the printer's own P2P protocol. Remote sending works away from
home over the internet, not just on the same LAN.

6. **Live camera — local network and remote**
720p H.264 camera streaming in the Device tab, hardware-decoded through VideoToolbox,
working both on the LAN and remotely.

7. **Stop, pause, and resume a running print**
Full print control, with a confirmation dialog on cancel and a completion dialog carrying
the real filename, duration, filament usage, and layer count.

8. **Local print history**
Every job is appended to `print_history.csv` in the app's profile directory: printer,
filename, start and end time, duration, filament used, layers, and outcome. Local only —
nothing is uploaded.

9. **Message Center**
The notification bell works, with correctly formatted dates, and deleting a message now
removes it from your account rather than just hiding it locally.

10. **Broker certificate pinning**
The connection to the printer message broker now verifies the server's certificate
instead of accepting any certificate presented. Previously the channel was encrypted but
unauthenticated.

11. **Printing workflow fixes**
Print now works from the slicer window, not just the Device tab. Choosing a printer sends
the job without a second confirmation step, and the app switches to the Device tab
immediately so you can watch the transfer.

12. **Interface fixes**
Redesigned Device tab layout, the window remembers its position, dialogs open centred on
the app rather than the screen, slicing tips are legible again, and the application menu
uses the correct app name.

13. **macOS packaging**
`make_app.sh` builds, bundles, signs, notarizes, and produces a DMG.

### Known not to work:

These are genuine gaps. Some are deliberate, some are unfinished.

1. **macOS on Apple Silicon only.** No Intel, Windows, or Linux builds come out of this
fork. The upstream project remains the place to get those.

2. **Camera light toggle and video quality selection do nothing.** The controls appear but
are not implemented — the necessary commands have not been worked out yet.

3. **Firmware updates (OTA) are not implemented.** The app will not check for or install
printer firmware. Use the official app or the printer itself.

4. **Multi-colour printers are untested.** This fork targets the M5 and M5C. The filament
slot-mapping step for multi-material machines is inherited from upstream and has not been
exercised.

5. **Switching the camera between two printers can drop the active stream.** Viewing one
camera at a time works reliably.

6. **Message Center error codes show as raw codes.** The lookup that turns an error code
into descriptive text is not fetched, so entries display the code itself.

7. **Usage analytics, feedback submission, and review prompts are disabled on purpose.**
This fork does not send usage data. The feedback form and app-rating prompts are inert.

8. **Only the US message broker's certificate is verified.** The EU broker uses the same
wildcard certificate name, but this has not been confirmed against the live EU host. If it
differs, monitoring will fail to connect with an explicit certificate error rather than
falling back to an unverified connection.

9. **Deleting a Message Center entry uses an endpoint recovered from the stock plugin.** It
behaves correctly in testing, but it is not from published documentation and could change.

10. **Extrude and retract are not hardware-verified.** They are implemented over the
printer's g-code channel and are believed correct, but have not been confirmed on a
machine.

11. **Remote connections rely on direct NAT traversal.** There is no relay fallback. On
networks where a direct path cannot be established, remote camera and remote sending will
not connect; monitoring and print control still work, since those go through the message
broker.

12. **Builds are unsigned unless you supply your own Developer ID.** An unsigned build runs
locally but will be refused by Gatekeeper if copied to another machine.

13. **The slicing engine is untouched.** No slicer behaviour, profile, or output has been
modified from upstream v1.5.26.
