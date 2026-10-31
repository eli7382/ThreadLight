THREADLIGHT–UNWEAVER DEVELOPER QA — PRIVATE DEBUG BUILD

Double-click Start-Wave-29.cmd (or the executable) to start at Wave 29.
Normal wave completion, collection and upgrade selection continue through Wave 35.
The actual Campaign Director uses a deterministic synthetic late-game runner prior,
then observes your real play. This supplies missing Waves 1–28 history; it is not
an AI mock. All combat, damage, rewards within the run, and physics are real.

Select-Scenario.cmd asks for wave, build, seed and (Wave 35) phase.
Supported waves: 7,14,21,28,29–35. Builds: offense, defense, mobility, mixed.
Late-game builds have 16 upgrade ranks and signature abilities; phase entries
start with full player health and a preset remaining boss HP value. The fixture
clock starts at the selected phase minute; ordinary fights never refill boss HP.
Wave 35 enters Phase II at 1:00, III at 2:00 and IV at 3:00 of simulated fight
time (after countdown, excluding pauses, including transitions). Zero HP ends
the fight immediately in any phase; later phases are not required for victory.
F5 restarts the current wave with its selected fixture build; direct Unweaver
fixtures restart the current phase. Restart reinitializes observation history.
P pauses; normal movement/dash and reward-choice controls remain unchanged.
Overlay: FPS, wave, phase, centi-HP, tactical style, selected tactical action,
attack stage, last radial family, radial occupancy, charge count and forecast
error. Late waves show Director review and changed deployment counts.
Start-Unweaver.cmd and Start-Phase-IV.cmd are direct shortcuts.
Test-Event-Loom.cmd forces the new portal/lattice attack for inspection. Use
Start-Unweaver.cmd for normal varied combat. Only the bright moving spindle
heads cause damage; the ghosted braided paths are warnings.
Each launcher saves unique diagnostic logs and seed/argument records under Logs.
Add -Performance to QA.ps1 to record frame timings alongside those logs.
Boss shrapnel and Spitter shots now continue to arena boundaries. Interception,
player collision and encounter/phase cleanup can still remove them. Distant
shots can remain while the boss moves or begins a validated follow-up.
Radial occupancy measures the major cast state, excluding phase transitions; it
does not include all later time that distant projectiles remain in flight.
Late waves update live movement roles and can review future deployments six
times, with faster reviews after substantial observed behavior changes.

PowerShell examples from this folder:
 ./QA.ps1 -Wave 35 -Phase 4 -Build defense -Seed 9
 ./QA.ps1 -Wave 32 -Build offense -Seed 42
 ./QA.ps1 -Wave 35 -Phase 2 -Mechanic 5 -Build mixed
 ./QA.ps1 -Wave 35 -Attack 1 -Build mobility
The executable also accepts existing --unweaver-attack 0..4 and
--unweaver-mechanic 0..5 (5 = Event Loom) with --unweaver-fixture mixed (see source report).

All scenarios, including direct EXE startup, suppress profile saving, legitimate
records and cosmetic unlocks. In-run sparks and upgrades still function.
No public practice menu is added. This separate Debug binary includes QA
instrumentation and a PDB. It is not the trusted Release game and its performance
is not representative of Release. No extra assets/DLLs are required.

Validation: 44/44 Debug CTest suites passed. Packaged launcher smoke runs cover
every requested starting wave and all four Unweaver phases with real gameplay.
These short automated launches verify startup/stepping, not full survival through
Waves 29–35 or human menu/input acceptance. Interactive QA remains yours.

Revision details and before/after evidence: UNWEAVER-EVENT-LOOM-QA.md.
Earlier experimental packages remain available for comparison. No merge authorized.
