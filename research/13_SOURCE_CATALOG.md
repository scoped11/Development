# Source catalog, evidence boundaries and bibliography

**Checked:** 12 November 2026 (research session). The entries below are source IDs used throughout the addendum. Official standards and platform docs support general semantics; publisher articles establish what the publisher said as of the indicated date, not independently verified efficacy; project repositories establish documented capabilities, not compatibility with every target. Some sources were accessed through indexed excerpts or redirected documentation rather than full specifications. The explicit further-reading entries and failed fetches are not treated as verified details. No private cheat forums, commercial cheat vendors, or undocumented game offsets are used as evidence.

## M — Windows, native binary and memory semantics

| ID | Source and direct link | Specific use |
|---|---|---|
| M01 | Microsoft, [Virtual Address Space](https://learn.microsoft.com/en-us/windows/win32/memory/virtual-address-space) | process-specific address space |
| M02 | Microsoft, [VirtualQueryEx](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualqueryex) | virtual page-region information |
| M03 | Microsoft, [ReadProcessMemory](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-readprocessmemory) | documented access requirements and failure semantics |
| M04 | Microsoft, [x64 calling convention](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention?view=msvc-170) | argument slots, alignment, saved registers |
| M05 | Microsoft, [x64 ABI conventions](https://learn.microsoft.com/en-us/cpp/build/x64-software-conventions) | type storage, alignment, ABI overview |
| M06 | Microsoft, [PE Format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format) | headers, section mapping, VA/RVA/file offset |
| M07 | Microsoft, [x64 Architecture Overview](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/x64-architecture) | register model and architecture context |

## H — Hardware and memory acquisition

| ID | Source and direct link | Specific use |
|---|---|---|
| H01 | Microsoft, [Kernel DMA Protection](https://learn.microsoft.com/en-us/windows/security/information-protection/kernel-dma-protection-for-thunderbolt) | DMA isolation and remapping, scope limitations |
| H02 | Linux kernel, [Dynamic DMA mapping Guide](https://docs.kernel.org/core-api/dma-api-howto.html) | CPU virtual, physical and bus addresses; mapping distinctions |
| H03 | FTDI, [Programming Guides](https://ftdichip.com/document/programming-guides/) | D3XX programmer's guide and separate D2XX interface |
| H04 | FTDI, [Application Notes](https://ftdichip.com/document/application-notes/) | AN_370 configuration, AN_375 loopback, AN_379 D3XX, AN_412 FT600/FT601 integration; use vendor-supported fixtures |
| H05 | Ulf Frisk, [LeechCore repository](https://github.com/ufrisk/LeechCore) | memory-source abstraction and release notes; no operational acquisition tests |
| H06 | Ulf Frisk, [MemProcFS repository](https://github.com/ufrisk/MemProcFS) | memory interpretation and forensic sources; version limitations |
| H07 | Microsoft, [Kernel DMA Protection for OEMs](https://learn.microsoft.com/en-us/windows-hardware/design/device-experiences/oem-kernel-dma-protection) | platform and firmware requirements, pre-boot protection |
| H08 | LeechCore project, [public API header in MemProcFS](https://github.com/ufrisk/MemProcFS/blob/master/includes/leechcore.h) | distinction between abstraction and source capabilities |

## E — Engines and networking

| ID | Source and direct link | Specific use |
|---|---|---|
| E01 | Unity, [IL2CPP scripting back end, Unity 6](https://docs.unity3d.com/6000.0/Documentation/Manual/IL2CPP.html) | compilation pipeline; documentation redirects to current Unity docs |
| E02 | Unity, [Managed code stripping](https://docs.unity3d.com/es/current/Manual/ManagedCodeStripping.html) | unused code removal and reflection caveats |
| E03 | Epic, [Unreal Engine Networking Overview](https://dev.epicgames.com/documentation/unreal-engine/networking-overview-for-unreal-engine) | actors, replication, server authority, replication systems |
| E04 | Godot, [High-level multiplayer](https://docs.godotengine.org/en/stable/tutorials/networking/high_level_multiplayer.html) | high/low-level networking distinctions |
| E05 | Valve, [Source SDK 2013](https://github.com/ValveSoftware/source-sdk-2013) | public native C++ research corpus; not Source 2 |
| E06 | Unity, [Netcode packages](https://docs.unity.com/multiplayer/netcode/netcode) | Netcode for Entities vs GameObjects, prediction |
| E07 | Unity, [Authority concepts](https://github.com/Unity-Technologies/com.unity.multiplayer.docs/blob/main/docs/terms-concepts/authority.md) | server and distributed authority models |
| E08 | Unity, [ManagedStrippingLevel API](https://docs.unity3d.com/kr/current/ScriptReference/ManagedStrippingLevel.html) | stripping level terminology |
| E09 | Unity, [Introduction to AssetBundles](https://docs.unity3d.com/6000.0/Documentation/Manual/AssetBundlesIntro.html) | asset packaging, engine/version scope |

## V — Graphics and math

| ID | Source and direct link | Specific use |
|---|---|---|
| V01 | Microsoft, [Coordinate systems and geometry](https://learn.microsoft.com/en-us/windows/uwp/graphics-concepts/coordinate-systems-and-geometry) | transform stages, viewports, vertices |
| V02 | Microsoft, [XMVector3Project](https://learn.microsoft.com/en-us/windows/win32/api/directxmath/nf-directxmath-xmvector3project) | explicit world/view/projection and viewport inputs |
| V03 | Microsoft, [XMQuaternionNormalize](https://learn.microsoft.com/en-us/windows/win32/api/directxmath/nf-directxmath-xmquaternionnormalize) | rotation normalization |
| V04 | Microsoft, [IDXGISwapChain::Present](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-present) | presentation and sync interval semantics |
| V05 | Khronos, [Vulkan specification](https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html) | **further reading only**; fetch timed out, no edition-specific claims attributed |

## R — Reverse engineering, reconstruction and forensics

| ID | Source and direct link | Specific use |
|---|---|---|
| R01 | Hex-Rays, [IDAPython API reference](https://python.docs.hex-rays.com/) | analysis, decompilation, xrefs, types, database APIs |
| R02 | Hex-Rays, [IDA Domain getting started](https://ida-domain.docs.hex-rays.com/getting_started/) | GUI/library mode and requirements |
| R03 | NSA, [Ghidra repository](https://github.com/NationalSecurityAgency/ghidra) | independent native analysis framework |
| R04 | ILSpy, [official repository](https://github.com/icsharpcode/ILSpy) | managed .NET decompilation |
| R05 | Hex-Rays, [IDA SDK IDAPython README](https://github.com/HexRaysSA/ida-sdk/blob/main/src/plugins/idapython/README.md) | plugin development documentation |
| R06 | Volatility Foundation, [Volatility 3](https://github.com/volatilityfoundation/volatility3) | memory forensics framework, offline source selection |
| R07 | wklin8607, [Il2CppDumper fork](https://github.com/wklin8607/Il2CppDumper) | project existence and docs; exact historical success from user notes |
| R08 | SamboyCoding, [Cpp2IL releases](https://github.com/SamboyCoding/Cpp2IL/releases) | version-specific candidate selection; do not assume latest stable supports every metadata |
| R09 | LukeFZ, [Il2CppInspectorRedux](https://github.com/LukeFZ/Il2CppInspectorRedux) | maintained reconstruction project and output categories |
| R10 | Hex-Rays, [IDA debugger API reference](https://python.docs.hex-rays.com/ida_idd/index.html) | thread-safety limitations; not used for target attachment |
| R11 | Hex-Rays, [IDA Domain project](https://github.com/HexRaysSA/ida-domain) | high-level abstractions and compatibility |
| R12 | NSA, [Ghidra Getting Started](https://github.com/NationalSecurityAgency/ghidra/blob/master/GhidraDocs/GettingStarted.md) | version-specific setup and supported platforms |

## C — Cryptography, compression and formats

| ID | Source and direct link | Specific use |
|---|---|---|
| C01 | NIST, [FIPS 197: AES](https://csrc.nist.gov/pubs/fips/197/final) | AES primitive, not mode |
| C02 | NIST, [SP 800-38A](https://csrc.nist.gov/pubs/sp/800/38/a/final) | block cipher modes |
| C03 | NIST, [SP 800-38D](https://csrc.nist.gov/pubs/sp/800/38/d/final) | GCM and GMAC |
| C04 | IETF, [RFC 8439](https://www.rfc-editor.org/rfc/rfc8439) | ChaCha20 and Poly1305, test vectors |
| C05 | IETF, [RFC 6229](https://www.rfc-editor.org/rfc/rfc6229) | RC4 known-answer vectors, legacy research |
| C06 | IETF, [RFC 5116](https://www.rfc-editor.org/rfc/rfc5116) | AEAD input/output interface |
| C07 | IETF, [RFC 8878](https://www.rfc-editor.org/rfc/rfc8878) | Zstandard framing and compression |
| C08 | IETF, [RFC 5869](https://www.rfc-editor.org/rfc/rfc5869) | HKDF extract/expand inputs |
| C09 | Google, [Protocol Buffers encoding](https://protobuf.dev/programming-guides/encoding/) | wire tags, varints, length-delimited fields |
| C10 | IETF, [RFC 1952](https://www.rfc-editor.org/rfc/rfc1952) | gzip file format |
| C11 | PKWARE, [ZIP APPNOTE 6.3.10](https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT) | ZIP record and directory structure; Nov 1 2022 revision |
| C12 | NIST, [FIPS 203: ML-KEM](https://csrc.nist.gov/pubs/fips/203/final) | KEM standard, Aug 13 2024; planning note Nov 17 2025 |
| C13 | NIST, [FIPS 204: ML-DSA](https://csrc.nist.gov/pubs/fips/204/final) | signature standard, Aug 13 2024; planning note July 31 2026 |
| C14 | NIST, [FIPS 205: SLH-DSA](https://csrc.nist.gov/pubs/fips/205/final) | stateless hash-based signature, Aug 13 2024 |

## G — Game publishers, policy and title-specific engineering

| ID | Source and direct link | Date / scope and specific use |
|---|---|---|
| G01 | Valve, [Counter-Strike 2 on Steam](https://store.steampowered.com/app/730/CounterStrike_2/) | official title/engine description; live product page |
| G02 | Riot, [Vanguard On-Demand](https://www.riotgames.com/en/news/vanguard-on-demand) | 2026 update, threat categories and publisher-reported measurements |
| G03 | Riot, [Addressing Cheating in VALORANT](https://support.riotgames.com/en-us/valorant/support-tools/addressing-cheating-in-valorant/) | current support page, unauthorized hardware/software |
| G04 | Activision, [RICOCHET overview](https://support.activision.com/articles/ricochet-overview) | current support page, covered titles and kernel/server layers |
| G05 | Activision, [Security and Enforcement Policy](https://support.activision.com/articles/call-of-duty-security-and-enforcement-policy) | enforcement definitions and policy |
| G06 | Epic, [Fortnite tournament anti-cheat requirements](https://www.fortnite.com/competitive/rules-guidelines/rules-library/new-fortnite-anti-cheat-pc-requirements-and-latest-legal-action?region=NAC) | Feb 10 2026, PC tournaments from Feb 19, Secure Boot/TPM/IOMMU |
| G07 | EA, [Apex competitive integrity update](https://www.ea.com/games/apex-legends/apex-legends/news/breach-anti-cheat-update) | Mar 4 2026, third-party input devices |
| G08 | EA, [Javelin coming to Apex Legends](https://www.ea.com/games/apex-legends/apex-legends/news/javelin-anticheat-announcement) | Sept 24 2026, announced Sept 29 PC transition |
| G09 | Ubisoft, [Rainbow Six Siege community checkpoint](https://news.ubisoft.com/en-us/article/4CHPYPn72TtqMJoYJRUU2m/rainbow-six-siege-community-checkpoint-new-anticheat-measures-balancing-updates-more) | Aug 7 2026, planned shared kernel system and evidence review |
| G10 | PUBG, [Anti-cheat update: distribution and ban evasion](https://pubg.com/en/news/11346?category=dev_notes) | Nov 5 2026, enforcement scope |
| G11 | PUBG, [Console anti-cheat update](https://pubg.com/en/news/11345?category=dev_notes) | Oct 29 2026, input and network abuse policy |
| G12 | Blizzard, [Defense Matrix: fair and fun](https://news.blizzard.com/en-us/article/24173281/defense-matrix-keeping-overwatch-2-fair-and-fun-for-every-player) | Overwatch 2 anti-cheat and unapproved peripherals |
| G13 | Riot, [Demolishing wallhacks with fog of war](https://www.riotgames.com/en/news/demolishing-wallhacks-valorants-fog-war) | Apr 14 2020, engineering design and information minimization; historical scope |
| G14 | Valve, [VAC ban support](https://help.steampowered.com/en/faqs/view/647C-5CC1-7EA9-3C29) | VAC ecosystem policy; not a private detector description |
| G15 | EA, [Apex controller enforcement and ban wave](https://forums.ea.com/blog/apex-legends-game-info-hub-en/controller-anti-cheat-enforcement--ban-wave-update--09-02-2026/13686028) | Sept 2 2026, DMA-related publisher-reported enforcement snapshot |
| G16 | Ubisoft, [Siege Player Protection hub](https://www.ubisoft.com/en-us/game/rainbow-six/siege/news-updates/player-protection) | ShieldGuard update history; release scope matters |
| G17 | Riot, [League patch 14.9 notes](https://www.leagueoflegends.com/en-us/news/game-updates/patch-14-9-notes/) | 2024 Vanguard deployment |
| G18 | Bungie, [Destiny 2 BattlEye support guide](https://help.bungie.net/hc/en-us/articles/4404072197140-BattlEye-Anti-Cheat-Support-Guide) | documented BattlEye and supported launch policy |
| G19 | EA, [Battlefield 6 FAQ](https://www.ea.com/games/battlefield/battlefield-6/faq) | Javelin integration |
| G20 | EA, [Battlefield 6 Season 4 anti-cheat update](https://www.ea.com/games/battlefield/battlefield-6/news/battlefield-6-anticheat-update-season-4) | Aug 28 2026, TPM, software/device policy and accessibility |
| G21 | EA, [Battlefield 6 July anti-cheat metrics](https://www.ea.com/games/battlefield/battlefield-6/news/battlefield-6-anticheat-metrics-july26) | Sept 4 2026, metric definition revision and false positives |
| G22 | Delta Force, [G.T.I. Security](https://www.playdeltaforce.com/en/anti-cheat.html) | ACE collaboration and enforcement policy |
| G23 | EA, [Battlefield 6 Secure Boot information](https://www.ea.com/games/battlefield/battlefield-6/news/secure-boot-information) | Aug 4 2025, PC Secure Boot requirement |
| G24 | EA, [Javelin 2026 progress update](https://www.ea.com/about/news/ea-javelin-anticheat-2026-update) | 2026 publisher-reported hardware/input detection and security updates |
| G25 | Riot, [Vanguard x LoL](https://www.leagueoflegends.com/en-au/news/dev/dev-vanguard-x-lol/) | 2024 publisher description of privacy/data collection; not an independent audit |
| G26 | Blizzard, [Defense Matrix reporting update](https://news.blizzard.com/en-us/article/23985150/defense-matrix-text-chat-changes-stopping-cheating-and-improving-reporting) | historical cheating and group-account policy |
| G27 | EA, [Battlefield 6 rules](https://help.ea.com/en/articles/battlefield/battlefield-6/play-by-the-rules/) | current conduct policy |
| G28 | Activision, [RICOCHET official site](https://www.callofduty.com/ricochet) | high-level defense-in-depth description |

## K — Windows kernel-mode drivers (WDK)

| ID | Source and direct link | Specific use |
|---|---|---|
| K01 | Microsoft, [Getting started with drivers](https://learn.microsoft.com/en-us/windows-hardware/drivers/gettingstarted/) | kernel driver architecture overview |
| K02 | Microsoft, [UMDF architecture](https://learn.microsoft.com/en-us/windows-hardware/drivers/wdf/user-mode-driver-framework-architecture) | user-mode driver framework role |
| K03 | Microsoft, [General driver samples](https://learn.microsoft.com/en-us/windows-hardware/drivers/samples/general-driver-samples) | Echo, IOCTL, ObCallback reference implementations |
| K04 | Microsoft, [Handling IRPs](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/handling-irps) | IRP major/minor functions and completion |
| K05 | Microsoft, [Dispatch routines and IRQLs](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/dispatch-routines-and-irqls) | dispatch IRQL by stack position |
| K06 | Microsoft, [Methods for accessing data buffers](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/methods-for-accessing-data-buffers) | METHOD_BUFFERED/DIRECT/NEITHER |
| K07 | Microsoft, [Driver security checklist](https://learn.microsoft.com/en-us/windows-hardware/drivers/driversecurity/driver-security-checklist) | IOCTL validation, NX pool, lab signing notes |
| K08 | Microsoft, [Security issues for I/O control codes](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/security-issues-for-i-o-control-codes) | IOCTL definition and bounds checking |
| K09 | Microsoft, [Managing hardware priorities](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/managing-hardware-priorities) | IRQL levels and API call restrictions |
| K10 | Microsoft, [ObRegisterCallbacks](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-obregistercallbacks) | handle create/duplicate pre/post callbacks |
| K11 | Microsoft, [Kernel-mode process and thread manager](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/windows-kernel-mode-process-and-thread-manager) | notify/callback discipline |
| K12 | Microsoft, [Implement HVCI-compatible code](https://learn.microsoft.com/en-us/windows-hardware/drivers/driversecurity/implement-hvci-compatible-code) | memory integrity constraints |
| K13 | Microsoft, [Kernel-mode code signing policy](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/kernel-mode-code-signing-policy--windows-vista-and-later-) | retail vs development signing |
| K14 | Microsoft, [PsSetCreateProcessNotifyRoutineEx](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-pssetcreateprocessnotifyroutineex) | process creation notify surface |
| K15 | Microsoft, [Driver Verifier](https://learn.microsoft.com/en-us/windows-hardware/drivers/devtest/driver-verifier) | stability testing on lab VMs |
| K16 | Microsoft, [IOCTL sample](https://learn.microsoft.com/en-us/samples/microsoft/windows-driver-samples/ioctl/) | four transfer types in one sample |

## P — Prior project evidence (not external sources)

- P01: User-supplied NOTES(1).md, historical project paths/tooling and intended tasks.
- P02: User-supplied PROGRESS(1).md, reported successful dump and previous/fresh build observations. Exact binary artifacts not supplied for independent verification.
- P03: User-supplied RESEARCH_DECRYPTION(3).md, prior standards research, proposed Cursor–IDA design, validation limitations.
- P04: User-supplied dma_cheats(1).md, internal_cheats(1).md, external_cheats(1).md, comparison(1).md; previous overview and assertions to audit rather than treat as authoritative.
- P05: Existing Game_Analysis_Research_Expanded/ collection, retained unchanged. This addendum supplements it.

## Fetch limitations and verification discipline

Valve Developer Community Source 2 and Source Multiplayer Networking URLs returned HTTP 403. The Source SDK repository and official Steam page were accessible; no claims are based on unseen wiki content. The Vulkan specification request timed out; V05 is clearly marked further reading. A direct FT601 product-page URL was unavailable, so FTDI programming and application-note indexes are the supporting references. A RenderDoc repository request failed; no RenderDoc-specific version claims are made. Not every linked page was read line-by-line; some were reviewed through search extracts or official index pages. The source list is not a claim of independent lab testing. Future edits should record exact retrieved version and section for every new claim, and use an archival copy when appropriate.

## Suggested citation format

Use `[M06, section name]` or `[G08, 2026-09-24]` in the text, preserving the direct URL in this catalog. For project findings use `[P02, section heading]` and label reported. For measurements use a run ID and raw output hash. The catalog does not turn a source's existence into verification of a target. Treat all proprietary layouts, unseen protection details, and target-specific keys/offsets as unresolved unless independently established under authorization.
