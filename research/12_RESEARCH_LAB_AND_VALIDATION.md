# Research laboratory and validation specification

Status: proposed test plan; no experiments executed. Inputs are invented fixtures for owned applications, local servers, offline files and manufacturer-supported hardware tests. This document defines what evidence would be required to promote claims. See 01–11 and source catalog. Never use a live protected game or public match as a test fixture.

## 1. Baseline lab inventory

| Component | Required inputs and records | Why |
|---|---|---|
| Workstation | OS build, CPU arch, RAM, disk, tool versions, time source | reproducibility and resource limits |
| Binary fixture | owned source, compiler version/options, executable, symbols, SHA-256 | ground truth for mapping |
| Engine fixture | engine version, project, build options, known scene | distinguish engine and game-specific behavior |
| Telemetry | schema, source role, timestamps, synthetic objects | coherent state and permission boundary |
| Network fixture | local server + test clients, random seed, controlled impairment | reproducible authority tests |
| Crypto fixture | known plaintext, test key/nonce, AAD, vector reference | exact expected values |
| Hardware fixture | supported vendor device, driver and library versions, authorized buffer/loopback | isolate transport and device faults |
| Review | evidence schema, acceptance criteria, result artifact | avoid claims from unsupported inference |

Maintain originals read-only. Separate input directory, generated output directory, logs and reports. Pin exact software builds; do not install unreviewed executables from random repositories. Keep credentials out of logs. Use OS protections and vendor-supported configurations. Record authorizations and stop boundaries before analysis.

## 2. Required manifest and run record

```json
{
  "fixture_id":"owned-native-v1",
  "authorization":"created by researcher for offline lab",
  "artifact":{"path":"fixtures/native/v1/sample.exe","sha256":"<actual hash>","bytes":0,"architecture":"x86-64","format":"PE32+"},
  "build":{"compiler":"<exact version>","flags":[],"source_revision":"<commit>"},
  "tool":{"name":"<tool>","version":"<exact version>","config_sha256":"<actual hash>"},
  "expected":{"record_layout":"SampleRecord","field_count":3},
  "status":"planned"
}
```

`bytes:0` is a placeholder, not a real executable size. Populate actual values before use; a manifest with placeholder values cannot substantiate findings. Run record: run_id, start/end monotonic and UTC timestamps, manifest hash, command or manual procedure, stdout/stderr path, exit code, outputs and hashes, validation result, unresolved errors, reviewer. Distinguish tool exit status from scientific validation; exit zero can still produce incorrect data.

## 3. Lab A — native ABI and PE mapping

**Objective:** distinguish file offset/RVA/VA/field offset; verify compiler layout. **Inputs:** own C++ fixture with struct `{uint8_t kind; uint32_t count; double reading;}`, source/debug symbols, x64 release and debug builds; separate synthetic PE mapping fixture in document 02. Add a second struct revision with a member inserted and alignment changed. **Procedure:** compile only own program, capture known `sizeof` and `offsetof` results, inspect PE headers, import into IDA/Ghidra, inspect relevant source-backed accessor, export instruction evidence. **Expected:** exact correct widths and displacements for the actual build; mapping example in 02 gives raw 0x520 for RVA 0x1120. **Negative cases:** virtual zero-fill tail, truncated file, wrong architecture, stale hash, ambiguous address kind. **Pass:** no invalid mapped file read, tool matches ground truth, version changes invalidate old evidence. **Output:** sections.json, layout.json, function evidence, version comparison; all records keyed by hash.

## 4. Lab B — telemetry snapshot consistency

**Objective:** prove coherent identity and bounded parsing in owned simulation. **Input:** JSONL telemetry with schema 1 and monotonic ticks. Fixture rows:

```json
{"tick":40,"id":5,"generation":1,"position":[0,0,0],"state":"active"}
{"tick":41,"id":5,"generation":1,"state":"destroyed"}
{"tick":41,"id":5,"generation":2,"position":[10,0,0],"state":"active"}
{"tick":42,"id":5,"generation":2,"position":[11,0,0],"state":"active"}
```

**Expected:** no velocity/history across generation 1 to 2; generation 2 history starts at its creation. **Negative inputs:** duplicate record, delayed tick, missing position, NaN encoded as invalid non-JSON numeric token, excessive count, schema version 99. **Pass:** explicit error/unknown states; no invented positions, bounded memory, dropped/stale records counted. **Output:** normalized replay records, error counts, maximum queue length, processing times. Run same fixture across engine adapters; schema must stay semantically consistent.

## 5. Lab C — graphics geometry and render timing

**Objective:** verify math and temporal alignment in an owned scene. **Input:** deterministic camera, viewport 1920x1080, clip `(0.5,-0.25,0.6,1)`, second viewport origin `(100,50)`, known camera FOV 60 degrees vertical at 16:9, object at fixed position, scene with known occluder. **Expected:** `(1440,675)` then `(1540,725)` for the clip sample, horizontal FOV about 91.5 degrees. **Negative cases:** w=0, NaN, outside clip, near-plane crossing, resize, camera and object at different ticks. **Pass:** projection agrees with engine projection within a declared numeric tolerance (e.g. 0.5 pixel for deterministic fixture); invalid samples not displayed; visible/hidden label compared to oracle. **Output:** frame_metrics.csv and screenshots of own scene, with producer tick and viewport generation.

Benchmark conditions: 60 Hz and 144 Hz rendering in owned app; fixed dataset and graphics settings; warm-up recorded separately; report p50/p95/p99, sample age, dropped frames and max queue depth. Avoid claiming measured performance until actual run outputs exist.

## 6. Lab D — server authority and inventory

**Objective:** validate ownership, sequencing, visibility and transaction invariants. **Input:** local owned server with players A/B and authorized spectator, deterministic scene, account ledger with one item. Synthetic commands: valid movement by A; mutation of B by A; duplicate transaction `tx-1`; two simultaneous consumes; NaN position; delayed acknowledgement; request spectator-only data by player. **Expected:** valid movement accepted; unauthorized state mutation rejected; at most one consume; duplicate no additional result; malformed command rejected; stale acknowledgement not applied; spectator-only data not disclosed. **Network conditions:** baseline, 80 ms extra RTT, 1% loss, reorder/duplication and reconnect, all via isolated emulator. **Pass:** invariants preserved under all profiles; legitimate valid inputs not rejected except explicitly documented behavior. **Output:** server event JSONL, ledger, network profile, reconciliation error summaries, visibility audit. No external network traffic.

## 7. Lab E — format recognition and authentication

**Objective:** distinguish parse failures from cryptographic failures. **Input:** LAB1 bytes in 07; gzip or Zstandard file produced from owned known payload; published AEAD vector using documented key, nonce, AAD, plaintext and expected ciphertext/tag. **Expected:** LAB1 v1 length 4 payload ABCD; valid compressed file produces exact original bytes; valid vector matches expected. **Negative cases:** truncated header, length overflow, unsupported flags/version, dictionary missing, output limit, altered ciphertext/tag/AAD, wrong key. **Pass:** errors classified correctly; no huge allocation; no unauthenticated plaintext accepted; hashes and vector references preserved. **Output:** per-case JSON with parse result, sizes, hashes, error offsets. No unknown game asset keys are required.

## 8. Lab F — hardware transport and permitted DMA buffers

**Objective:** distinguish enumeration, transport, device logic and data validation. **Input:** vendor-supported board and fixture, supported driver/library versions and official test app; known byte patterns in an authorized device buffer or documented loopback (not arbitrary process memory). Keep IOMMU/platform protections enabled. **Steps:** record inventory, run manufacturer-supported enumeration/loopback, validate expected fixture bytes and errors, measure elapsed time/goodput only for the permitted fixture. **Negative cases:** disconnected device, unsupported driver, timeout, deliberately malformed known test frame where vendor fixture supports it. **Pass:** no unsupported platform changes, no arbitrary RAM acquisition, exact bytes checked, error state distinguishes transport from parser. **Output:** configuration tuple, manufacturer log, loopback result, transferred bytes, verified bytes, errors, elapsed time. A pass does not imply unrestricted memory access or end-to-end behavior in another environment. See H01–H07 and 01.

## 9. Lab G — Cursor/IDA integration

**Objective:** enforce read-only, evidence-producing API on owned database. **Inputs:** two owned fixture databases, hash manifest, request/response schemas in 11, mock backend, maximum response sizes, approved local endpoint. **Cases:** status while busy, identity mismatch, invalid address, pagination, unsupported operation, missing auth, missing decompiler, interrupted request, binary text containing instructions, attempted DB mutation. **Expected:** all unauthorized/invalid calls fail before dispatch; identity included in successful and partial results; static DB not modified. **Pass:** mock contract and actual backend agree; no unbounded requests; one serialized heavy call; no stale result reuse. **Output:** contract_test.json, tool version, integration log, evidence export. Implementation and actual execution remain pending.

## 10. Lab H — KMDF driver and IOCTL contract (see also chapter 14)

**Objective:** on an isolated test VM with documented CI/HVCI state, build and exercise a signed-or-test-signed KMDF Echo-derived driver with a bounded custom IOCTL (synthetic ECV1 contract in [14_KERNEL_DRIVERS.md](14_KERNEL_DRIVERS.md)). Optional: Microsoft ObCallback sample for handle-stripping demonstration only—not on a protected game image. **Input:** WDK version, VM snapshot, Driver Verifier profile, `.sys` and catalog hashes. **Expected:** IOCTL success and rejection cases recorded; Verifier clean for declared fuzz window. **Pass:** manifest includes signing thumbprint and OS build; no circumvention steps. **Status:** proposed; not executed.

## 11. Lab I — kernel load forensics vs EAC/BE threat model (see chapter 16)

**Objective:** on an isolated VM with manifest HVCI/blocklist state, compare kernel artifacts (module list, unload list, and other **observable** load bookkeeping) before and after a **synthetic** BYOVD map using `mapper_nal --rw-only`—without launching a protected multiplayer title. **Pass:** graded diff report linked to [16_EAC_BE_KERNEL_RESEARCH.md](16_EAC_BE_KERNEL_RESEARCH.md) §4–§6. **Status:** proposed; not executed.

## 12. Evidence grading and review checklist

| Grade | Definition | Example |
|---|---|---|
| Standard fact | official specification establishes semantics | PE RVA definition |
| Publisher-reported | publisher describes its system or result | announced anti-cheat transition |
| Reported project | user's historical record, not independently checked | successful dumper run |
| Heuristic | possible pattern, alternatives exist | recognizable magic/constant |
| Statically verified | exact artifact and instructions establish claim | owned field access width |
| Fixture-verified | controlled execution matches ground truth | known parse output |
| Unresolved | required proof absent or conflicting | unknown runtime material |

Before promoting a claim: verify source, date, scope, exact hash, tool version, address kind, independent evidence and alternative hypotheses; record negative results. Maintain a separate issue log with owner, dependency and next validation. Do not quietly replace historical claims. Publish source links and clear limits. No lab has been executed for this research edition.
