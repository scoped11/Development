# Unified game-cheat architecture and systems engineering reference

**Research edition:** 12 November 2026 | **Scope:** general, cross-title architecture, defensive analysis, and authorized laboratory systems. Not a recommendation of a method for cheating. No target-specific implementation, hidden-state acquisition, injection procedure, or evasion instructions. Sources: [13_SOURCE_CATALOG.md](13_SOURCE_CATALOG.md). All examples are illustrative, not measured.

## 1. Classification: what changes, where it runs, and what it consumes

Three axes are independent: execution location (same process, another process, kernel, separate hardware), information source (engine state, authorized telemetry, pixels, user input, network messages), and effect (display, automation, local state change, server transaction). Calling a product a “DMA cheat” describes a possible data path, not its features or the trustworthiness of its output. Computer vision may run on the same or another computer without reading process memory. Input adapters may automate physical input without knowing anything about the game state. A hybrid can use multiple sources. The same game feature can appear in multiple architectures.

| Architecture | Execution context | Data boundary | Legitimate analogue | Security / reliability concerns |
|---|---|---|---|---|
| Internal | Application address space | Native objects and approved API | Engine editor extension, profiling plugin | ABI mismatch, lifecycle, thread affinity, corruption, integrity |
| External | Separate user-mode process | Explicit IPC, exported telemetry, or OS-authorized inspection | Debugger, spectator dashboard | Access rights, stale data, sampling, versioning |
| Kernel-level | Privileged OS component | Kernel-managed resources | Signed device driver, security telemetry | System-wide compromise, IRQL, signing, fault propagation |
| DMA | Bus-mastering peripheral and host-side software | Device address mapping, authorized buffers | NIC, GPU, acquisition appliance | IOMMU, device/driver compatibility, transport errors, mapping validity |
| Computer vision | CPU/GPU image processing | Captured pixels | Accessibility, visual QA, replay analysis | Occlusion, false detections, latency, privacy |
| Input automation | Application, OS or peripheral | Input event stream | Accessibility and QA harness | Focus, device policies, timing, unintended effects |
| Network/logic abuse | Client or backend interface | Protocol and authority boundary | Load testing, protocol fuzzing in own server | Authorization, state invariants, replay, abuse |

Riot explicitly distinguishes DMA, kernel abuse and pixel-based automation in its public threat reporting [G02]. These categories are not a detection-risk ranking. No architectural placement establishes invisibility, immunity from sanctions, or permission to act.

## 2. Internal software architecture (authorized in-process plugins)

In an authorized host, an extension shares address space and may use documented interfaces. That makes passing references efficient, but couples it to the host's type, ownership, thread, and lifecycle rules. A pointer is not a lifetime guarantee. Engine objects may be destroyed or recycled between frames, and the host may replace a scene, world or device without unloading the extension.

A well-structured legitimate plugin separates:

- **Adapter:** translates engine SDK objects into a stable application-owned schema; checks interface versions.
- **Lifecycle:** initialize, subscribe, update, unsubscribe, release resources; idempotent teardown.
- **Data model:** immutable, bounded snapshots with generation identifiers; no raw host pointers crossing ownership boundaries.
- **Presentation:** debug draw using supported editor/render APIs, with per-frame resource ownership.
- **Diagnostics:** version, thread, exception or error context, queue size and processing time; redact personal data.

Microsoft's x64 ABI [M04, M05] establishes register positions, stack alignment, aggregate passing and preservation requirements. For a synthetic signature `f(int, double, int, float)` the positions map to RCX, XMM1, R8, XMM3 under the ordinary Windows x64 convention. A hidden return buffer can alter apparent argument placement. Incorrect assumptions can corrupt registers or stack state even when a function appears to return correctly. Do not transfer an ABI to ARM64 or System V AMD64 by analogy.

### Threading, lifetime and graphics

The simulation, rendering, physics and IO threads need not run together. An engine's documented scheduling mechanism should marshal work to the correct thread; locks should not cover GPU waits or blocking IO. GPU command completion and CPU submission are different events. Device-loss recovery should rebuild only resources tied to the lost device, not reinitialize unrelated state. A per-frame telemetry object can be copied to a bounded queue; if consumers fall behind, mark dropped snapshots rather than accumulating unbounded latency.

### Inputs and outputs for a lab plugin

Input: owned test project's source, SDK version, compilation target, debug symbols, known scene, and a frame callback. Output: versioned JSON telemetry, a debug panel, frame count, and lifecycle log. Acceptance: repeated scene reloads do not leave subscriptions; shutdown frees resources; unavailable object returns a missing-state record, not a dangling reference. No commercial process injection is required.

## 3. External software architecture (authorized diagnostics)

An external diagnostic tool runs in another address space. Preferred interfaces are authenticated local IPC, engine telemetry export, logs, replay files, and developer-provided APIs. This reduces dependence on memory layout, offers explicit versioning, and gives the producer control of information exposure.

Windows grants process access through handles; `ReadProcessMemory` requires the relevant read right, and inaccessible ranges fail [M01, M03]. `VirtualQueryEx` reports properties of mapped regions [M02]. Neither is an entitlement to examine a protected process. A separate process can still affect system resources and can fail independently of the host. Kernel privileges are not a substitute for an application protocol.

### Producer-consumer contract

Example owned-game telemetry record:

```json
{"schema":1,"session":"lab-1","tick":120,"monotonic_ns":2000000000,"objects":[{"id":17,"generation":3,"position":[1,2,3],"state":"active"}]}
```

The consumer should validate message length, schema, finite numeric values, uniqueness of `(id,generation)`, and monotonic ordering. An object removed at tick 121 must not reappear from a delayed tick 120 packet. Different clock domains require correlation; wall-clock time is unsuitable for measuring short intervals if adjusted by the OS. A stale snapshot should be labeled stale rather than displayed as current. Bound memory usage and maximum entity counts before deserialization.

In an owned test process, distinguish a legitimate zero value from a failed read. Use status, byte count, version and provenance rather than filling failure with zero. Explicitly handle process exit and restart, where the same executable may load at another base and identifiers may be reused.

## 4. DMA architecture: full hardware and software stack

### The physical and logical layers

1. Host CPU, memory controller and physical RAM. The CPU's process virtual-address space is not a device address space.
2. PCIe root complex and links. Devices are enumerated through configuration space, assigned resources, and may issue memory transactions according to platform policy.
3. Endpoint (often an FPGA). FPGA logic may implement a PCIe endpoint, packet handling, buffer management, clocks and a bridge protocol. Firmware, host driver, and FPGA bitstream are different artifacts.
4. Transport bridge (e.g. FT60x). FTDI FT600/FT601-series chips provide a USB-to-parallel-FIFO transport; D3XX is the matching software interface [H03,H04]. USB enumeration only establishes some transport functionality, not validity of another subsystem.
5. Acquisition library. A source abstraction can represent a memory dump, a VM snapshot or an authorized physical acquisition device. LeechCore describes this role [H05]; MemProcFS provides interpretation of memory sources [H06]. Neither creates authorization or guarantees complete memory.
6. Interpretation/application. Translation, file parsing, schemas, timestamps, errors and analysis reports belong here. A renderer or a display compositor is downstream; it cannot repair bad data.

### Address spaces and protection

CPU VA -> page translation -> CPU physical address. Device-issued bus address/IOVA -> IOMMU or host bridge -> permitted physical region. CPU and device translations are separate [H01,H02]. Device-accessible memory can be intentionally restricted to driver-owned buffers. Windows Kernel DMA Protection relies on memory remapping and platform policy; firmware also has obligations during boot [H01,H07]. A PCIe endpoint is not automatically able to access every page of RAM. Memory encryption, absent pages, device policy and acquisition consistency may impose further limitations.

### PCIe and USB throughput: units matter

For PCIe Gen2 x1, the nominal signaling rate is 5 GT/s, with 8b/10b encoding; the approximate decoded line-rate ceiling is 4 Gbit/s or 500 MB/s per direction before protocol overhead. This is not an observed data rate. Packet headers, completions, outstanding request limits, link errors, device logic, USB flow control, library overhead and CPU scheduling all affect useful throughput. A USB 5 Gbit/s label is likewise signaling rather than delivered application bytes. A serial pipeline can only sustain the effective throughput of its slowest stage. Simultaneous outstanding work can change measured latency and throughput without changing physical line speed.

Illustrative lab transfer: 64 MiB delivered in 0.5 s -> 128 MiB/s. If 2 MiB fail validation, report both delivered 128 MiB/s and verified goodput 124 MiB/s. Specify MiB (2^20) vs MB (10^6), transfer size, run count and error rate. Do not infer arbitrary-memory performance from a loopback test.

### FPGA/USB compatibility and troubleshooting matrix

| Observed symptom in an authorized fixture | Evidence to collect | What it does NOT establish |
|---|---|---|
| PCIe device absent | Slot/link status, motherboard documentation, supported device inventory | A particular firmware defect |
| USB bridge absent | Cable, USB port enumeration, vendor driver status | PCIe endpoint failure |
| USB bridge listed, open fails | Exact driver/library architecture, error code, supported device revision | Ability to read host memory |
| Device opens, loopback fails | Official loopback result, version tuple, transport errors | Invalid host page translations |
| Loopback passes, fixture transfer fails | Driver-authorized buffer registration, fixture status, IOMMU events | That protection should be disabled |
| Transfer succeeds, parser rejects data | Schema, endianness, size, checksum, sample identity | Hardware failure |
| Intermittent disconnect | Power, temperature, cable quality, logs, controlled repeat | A single definite root cause |

Record hardware revision, exact firmware provenance and hash, driver version, DLL version and bitness, OS build, test fixture and result separately. LEDs are vendor-specific indicators, not protocol proofs. A firmware programming utility reporting success shows its programming operation, not end-to-end correctness. Preserve platform protection and use supported manufacturer diagnostics rather than modifying device identities or security configuration.

## 5. Kernel software and privilege boundary

A kernel driver has a wider fault domain than a user-mode process. Unvalidated IOCTL buffers, pointer lifetime, concurrency and device-removal errors can cause system failures. Legitimate drivers should have narrow documented interfaces, least privilege, device ACLs, signing, bounded inputs, cleanup and security review. Do not infer that driver presence confers arbitrary rights; operating system code-integrity policy and virtualization-based security may restrict loading and operation. Platform protection must remain enabled in the lab. [H01,H07,G04]

## 6. Pixel-based analysis and input devices

Vision uses image evidence rather than object identity. A frame contains visible color samples but not necessarily object depth, team, off-screen state, server health or future movement. Different UI scaling, HDR, resolution, compression, motion blur and visual effects can affect an image model. An offline replay classifier should report precision, recall, confidence, unknown classes and frame timestamps. Train/test splits should be by session or map, not adjacent frames, to avoid leakage.

Input hardware can emit events independent of internal memory. This is not an exemption from rules. EA's official Apex guidance and PUBG's console updates explicitly address prohibited automation and input manipulation [G07,G11]. An authorized accessibility feature or approved peripheral must be evaluated against the specific publisher policy; not all accessibility use is equivalent to cheating. This reference does not provide automated aiming or recoil input generation.

## 7. Comparison: engineering, not marketing

| Criterion | Internal | External | DMA | Vision-only |
|---|---|---|---|---|
| Semantic data | SDK-dependent | Protocol/permission-dependent | Interpretation-dependent | Visible pixels only |
| Timing | Host scheduling | IPC/sampling | Transport + acquisition + parsing | Capture + inference |
| Fault domain | Host process | Separate process, possibly shared system | Device/driver/host + reader | Capture/analysis system |
| Version coupling | API/ABI | Schema or layout | Source, mappings, layout and transport | Visual distribution |
| Deployment | Approved plugin | Authorized app | Supported hardware fixture | Offline media or approved capture |
| Security relevance | Integrity/lifecycle | OS access control | IOMMU/platform isolation | Input and behavior policy |

There is no universal fastest option or ban-risk ordering. No client-only architecture can disclose data not sent to the client by a correctly designed server. Hardware isolation and data minimization complement each other. See 06 and 10.

## 8. Required evidence package

Use an owned sample application, explicit authorization, baseline state, documented API/fixture, version manifest, synthetic known bytes, timestamps, expected output, failure cases and logs. Separate link speed from throughput, availability from correctness, and analysis results from inferred capability. No target-specific acquisition or input control was implemented or tested for this document.
