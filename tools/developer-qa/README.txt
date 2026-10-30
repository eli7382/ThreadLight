THREADLIGHT–UNWEAVER DEVELOPER QA — PRIVATE DEBUG BUILD

Double-click Start-Wave-29.cmd (or the executable) to start at Wave 29.
Normal wave completion, collection and upgrade selection continue through Wave 35.
The actual Campaign Director uses a deterministic synthetic late-game runner prior,
then observes your real play. This supplies missing Waves 1–28 history; it is not
an AI mock. All combat, damage, rewards within the run, and physics are real.

Select-Scenario.cmd asks for wave, build, seed and (Wave 35) phase.
Supported waves: 7,14,21,28,29–35. Builds: offense, defense, mobility, mixed.
Late-game builds have 16 upgrade ranks and signature abilities; phase entries
start with full player health and boss vitality at the selected phase boundary.
F5 restarts the current wave with its selected fixture build; direct Unweaver
fixtures restart the current phase. Restart reinitializes observation history.
P pauses; normal movement/dash and reward-choice controls remain unchanged.
Overlay: FPS, wave, phase, centi-HP, tactical style, radial family, named attack
stage. The family is the selected
radial family, not necessarily the currently executing unique mechanic.

PowerShell examples from this folder:
 ./QA.ps1 -Wave 35 -Phase 4 -Build defense -Seed 9
 ./QA.ps1 -Wave 32 -Build offense -Seed 42
The executable also accepts existing --unweaver-attack 0..4 and
--unweaver-mechanic 0..4 with --unweaver-fixture mixed (see source report).

All scenarios, including direct EXE startup, suppress profile saving, legitimate
records and cosmetic unlocks. In-run sparks and upgrades still function.
No public practice menu is added. This separate Debug binary includes QA
instrumentation and a PDB. It is not the trusted Release game and its performance
is not representative of Release. No extra assets/DLLs are required.

Validation: 42/42 Debug CTest suites passed. Packaged launcher smoke runs cover
every requested starting wave and all four Unweaver phases with real gameplay.
These short automated launches verify startup/stepping, not full survival through
Waves 29–35 or human menu/input acceptance. Interactive QA remains yours.
