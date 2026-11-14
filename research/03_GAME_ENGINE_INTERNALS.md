# Cross-engine internals: Unity, Unreal, Source, Godot and custom engines

This is a comparative architecture and research guide, not a target-specific memory map. The public SDK or manual documents its own version, not the internal layout of a shipped commercial title. Sources E01–E09, R01–R09.

## 1. Engine identification and artifacts

Identify platform, executable format, runtime and build before choosing a tool. A directory name or string is a lead. Confirm through multiple artifacts: module identity, documented build output, imports, metadata format, version information, or publisher documentation. A title can have several binaries, runtime layers and platform-specific engines. A launcher and engine executable are not interchangeable. Engine source and game-specific application logic are different layers.

| Engine family | Core model | Typical authorized research artifacts | Major caveat |
|---|---|---|---|
| Unity Mono | Managed objects and IL runtime | owned managed assembly, symbols, project | IL and JIT/native code differ |
| Unity IL2CPP | managed declarations converted to native code | matching binary + metadata + symbols if available | stripping, sharing and platform layout |
| Unreal | UObject hierarchy, reflection, actors/components | source project, reflected declarations, compiled module | generated metadata is not full layout |
| Source (2013 SDK) | native client/server entity architecture | Valve SDK and owned mod | not a Source 2 SDK |
| Source 2 | newer Valve engine family | official tools, demo documentation, authorized projects | do not transfer old Source assumptions |
| Godot | scene tree, nodes/resources, scripting | own project, engine source, export | script/runtime/platform differences |
| Custom | implementation-specific | authorized binary, symbols, schema | no universal type or offsets |

## 2. Unity: managed and native representations

Unity's IL2CPP pipeline translates managed IL into C++ and compiles native output [E01]. UnityLinker can remove unused managed code [E02]. Consequently, the source-facing type catalog, generated metadata, native instructions and runtime objects are not four interchangeable copies of the same information. On typical Windows IL2CPP builds, GameAssembly.dll and global-metadata.dat are relevant artifacts, but names, packaging and protection vary. Match exact build hashes; do not use mismatched pairs.

**Managed model:** C# fields, properties, methods, generics, arrays and references. A property may call arbitrary code and may lack a stored backing field. A value type may be embedded in another object; a boxed value has different representation. Strings, managed references and native engine resource handles have separate lifetime semantics. The native Unity object behind a managed wrapper is not the same thing as the wrapper.

**Native model:** compiled code, runtime metadata, generic instantiations and native engine interfaces. Generated methods can be shared, folded, inlined or stripped. A method table entry can refer to a wrapper/thunk rather than the primary code body. Static storage belongs to class/runtime semantics, whereas instance storage belongs to a particular object. Do not assume a static-storage offset is universally fixed by metadata version.

**ECS:** Unity DOTS separates components from identity, and third-party game-specific ECS frameworks may be quite different. A source-level `System` updates components over time; the layout can be archetype- or application-specific. Query results can change when entities migrate. GameObject/MonoBehaviour and ECS models can coexist.

### Tool choice and metadata v39

| Tool | Input | Appropriate use | Caveat |
|---|---|---|---|
| ILSpy [R04] | managed assemblies | CIL/metadata browsing | not a native IL2CPP decompiler |
| wklin8607/Il2CppDumper [R07] | matching IL2CPP artifacts | reconstruction for supported versions | user log reports v6.7.48 succeeded, not retested here |
| Cpp2IL [R08] | supported binary/metadata | independent reconstruction | inspect release notes, pre-release vs stable |
| Il2CppInspectorRedux [R09] | supported pair | type/native mappings and disassembler integration | version claim is not a guarantee for modified formats |
| IDA/Ghidra [R01,R03] | native module | independent instruction/flow evidence | requires correct loader, base and type assumptions |

The prior `RESEARCH_DECRYPTION.md` identifies Cpp2IL `2022.1.0-pre-release.21` and Il2CppInspectorRedux as version-39 candidates. Its historical notes describe the wklin fork as used successfully in the project. Treat as reported tool compatibility; do not claim all metadata v39 variants are equivalent. Use a benign owned Unity IL2CPP build with known source to evaluate a chosen release; record output and disagreements.

## 3. Unreal: reflection, actors, ownership and replication

Unreal's public model includes UObject-derived objects, actors, actor components, reflected properties and functions, and worlds/levels. A UObject is not necessarily an actor or spatial object. Actor components can be attached; scenes and worlds can change during map travel. A reflected property is metadata used by systems such as serialization and replication, not proof that every native field is reflected. C++ compiler layout and generated reflection data have separate roles.

Networking documentation describes authoritative server state and replication to relevant clients [E03]. An actor may be local only, replicated, owned by a connection, dormant, or represented by a subobject. Replication can send properties and remote procedure calls; relevance and scheduling affect what arrives. An RPC request is not equivalent to permission for a state change. The server should validate context, ownership and game rules. A replicated attribute's local value may be predicted or delayed.

### Owned-project study plan

Inputs: UE version/commit, C++ source of a test actor and component, generated headers, debug symbols, local two-player project, network emulation profile. Outputs: class/property inventory, actor lifecycle log, authority-role observations, replication log and local vs authoritative state. Acceptance: object destruction, map transition, packet loss, rejected invalid command, and schema migration are handled. Do not infer from this experiment the live internals of Fortnite, VALORANT, PUBG or another Unreal-based title.

## 4. Valve Source and Source 2

Valve's Source SDK 2013 is a valuable native C++ study corpus [E05]. It is a public source-era SDK, not Counter-Strike 2's implementation. Use the code to learn how client and server libraries may share declarations, entity update logic, and game-side simulation. The source tree provides controlled code for reading compiler output and verifying native types. Study only SDK-permitted builds and mods.

Source 2 is a newer engine family; Counter-Strike 2's official game materials are the appropriate publisher-level starting point [G01]. The public SDK does not establish current entity-list layout, schema system details, build-specific offsets, netvar names or protected-client behavior. Valve's developer wiki endpoints were inaccessible during this research (403); no assertion depends on their unseen contents. Demo playback and official community tools are preferable artifacts for title-specific research. See 09 for title dossiers.

## 5. Godot: scenes, nodes and scripts

Godot organizes application behavior around scenes/nodes and resources. A resource can be shared by multiple nodes; node lifetime follows scene transitions and tree ownership. GDScript, C# and native extensions can coexist, with different analysis surfaces. The public high-level multiplayer API provides a structured way to synchronize state, whereas lower-level transports leave much of the protocol to application code [E04]. Authority is a design choice in the project, not something established merely by selecting an engine.

Lab: own a project with two nodes, scripted state, explicit RPC authority and replay export. Compare the scene tree, source and native extension build. Observe which state is locally generated and which arrives over the network. Output a documented schema and event timeline, not guessed private layouts.

## 6. Custom engines, browser games and other runtimes

Native proprietary engines may expose no public type model. Treat file formats and module boundaries separately; public patents, talks and SDK documentation provide context but cannot establish exact offsets. Web games may use JavaScript, WebAssembly, WebGL/WebGPU and a remote backend; DOM state, WebAssembly linear memory and server state are different boundaries. JVM or .NET games may use bytecode plus JIT-compiled code. Mobile ARM64 builds have different ABI, packaging and memory protections from Windows x64.

For a new engine, begin with authorized source/build artifacts, architecture, symbols, file format and lifecycle. Locate official documentation for the exact version, compare API design to actual compilation output, write evidence records, then establish any target-specific claims only with permission and reproducible observations.

## 7. Cross-engine semantic normalization

Define schema independent of engine objects: stable ID + generation, position with explicit coordinate frame/units, orientation convention, state enum, validity interval, and provenance. A Unity position in meters, Unreal project using its usual centimeters, and a custom unitless grid must not be compared without explicit conversion. Camera matrices, physics positions and network snapshots may represent different timestamps. A common adapter interface should expose unavailable fields rather than inventing defaults. Use offline, owned fixture scenes to verify conventions.

## 8. Required evidence and unresolved questions

For any title dossier record platform, release/build, engine claim source, security/publisher policy, authorized artifacts, available APIs/replay and unknowns. Separate engine-level statements from publisher-specific facts. The existing Rob Run notes are an example of build-specific evidence, not the starting definition for all engines. No new title binaries were inspected and no engine experiments were executed for this addendum.
