# Popular-game security and architecture case studies

**Research snapshot: 12 November 2026.** This is cross-title public documentation research. The cases distinguish what the publisher confirms from genre-level engineering questions. No proprietary binaries inspected, no live offsets, no private data or circumvention methods. See source catalog G01–G22; dates refer to publisher statements, not independently measured deployment. Security systems change by season, region and platform.

## 1. Comparison matrix

| Game | Genre | Publicly documented security | Key research theme | Public evidence |
|---|---|---|---|---|
| Counter-Strike 2 | Tactical FPS | Valve VAC ecosystem | replay and build identity; Source vs Source 2 | G01,G14 |
| VALORANT | Tactical FPS | Riot Vanguard; fog of war | data minimization, hardware/input threats | G02,G03,G13 |
| Fortnite | Battle royale | tournament platform checks | Secure Boot, TPM, IOMMU | G06 |
| Call of Duty / Warzone | FPS/BR | RICOCHET | layered analytics, client/server and kernel | G04,G05 |
| Apex Legends | Hero BR | EA Javelin transition announced Sept 29, 2026 | controller/DMA threats and version drift | G07,G08,G15 |
| Rainbow Six Siege | Tactical FPS | ShieldGuard, announced shared kernel initiative | evidence review, input and AI threats | G09,G16 |
| PUBG: Battlegrounds | BR | publisher anti-cheat program | PC distribution, console input and latency abuse | G10,G11 |
| Overwatch 2 | Hero FPS | Defense Matrix program | automation and account/group abuse | G12 |
| League of Legends | MOBA | Vanguard deployment in patch 14.9 | scripting, botting and fog | G02,G17 |
| Destiny 2 | Online action RPG/FPS | BattlEye | live service client policy | G18 |
| Battlefield 6 | Large-scale FPS | EA Javelin | platform integrity, metrics quality | G19–G21 |
| Delta Force | FPS/extraction | ACE + G.T.I. Security | competitive and economy trust | G22 |

## 2. Counter-Strike 2

**Publisher evidence:** Valve's official CS2 site [G01] and VAC support documentation [G14]. The 2013 Source SDK [E05] is a separate public codebase, useful as a native study corpus, not a CS2 implementation. Do not treat legacy Source classes or offsets as Source 2 facts.

**Research questions:** How are match timing, replay viewpoint, weapon outcomes, server validation and player visibility represented in permitted replay data? Can a clip be reproduced under different viewpoint/latency conditions? What version produced a demo, and is a parser compatible? Use official/demo tooling or public SDK fixtures. The exact current protected-client class layouts, schemas and VAC detection rules are not established here.

## 3. VALORANT

Riot's April 2020 engineering article [G13] describes a fog-of-war approach for limiting wallhack information; its 2026 Vanguard update [G02] discusses DMA, kernel abuse and pixelbots. Riot's support page [G03] treats unauthorized input automation and hardware as cheating. The central engineering lesson is that reducing hidden client data changes the usefulness of a memory-based attack independently of the memory acquisition mechanism.

Study in a local synthetic tactical scene: two players, visibility transitions, authoritative messages, delayed/reordered updates, and spectator role. Assert private opponent state is not sent when disallowed. Use official public recordings for replay-level observations only. Unknowns: current exact relevance implementation, client layout and enforcement details. No claim that every hidden actor is absent at every tick.

## 4. Fortnite

Epic announced Feb 10, 2026 that all PC tournaments would require Secure Boot, TPM and IOMMU from Feb 19 [G06]. The article specifically describes memory-access protection against cheat hardware. The scope is tournament PC requirements, not necessarily every mode or platform. Unreal Engine public documentation [E03] explains replication generally; it does not specify Fortnite's live replication layout.

Research theme: trust boundaries across boot, peripheral memory and server-controlled state. Use an owned Unreal project or permitted creator tooling to study public actor replication and inventory. Do not infer a security mode or requirement for another title from this policy.

## 5. Call of Duty / Warzone

Activision's RICOCHET overview [G04] describes server analytics and a PC kernel driver for supported titles; its enforcement policy [G05] addresses unauthorized data manipulation, automation and devices. The franchise has multiple releases, modes and platforms. Keep exact edition and date rather than calling all of them one binary. Security announcements are publisher statements, not a full detector specification.

Research theme: combine server event telemetry, client integrity and account investigation rather than a single detector. Own-server experiment: validate movement, shot requests, match results and retransmissions. Measure false positives on ordinary network loss. Unknowns: actual proprietary game engine structures and detection thresholds.

## 6. Apex Legends

EA announced September 24, 2026 that PC would switch from Easy Anti-Cheat to EA Javelin September 29 [G08]. EA's March 4 article [G07] clarifies unauthorized peripheral automation policy; its September 2 update [G15] reported 290 DMA-related bans in a seven-day enforcement snapshot. That is publisher-reported enforcement, not a detection probability or independently audited population rate. Do not continue treating an old EAC description as current without checking release date.

Research theme: hero/ability state, inventory, cross-platform input policy, and the lifecycle of security systems. Use authorized replay clips for observation; separate ability effects from input timing and server outcome. No engine offsets or exact transition coverage independently confirmed here.

## 7. Rainbow Six Siege

Ubisoft's Aug 7, 2026 checkpoint [G09] describes a new shared kernel-level system planned for Year 11 Season 3, policies covering device spoofing and botting, and manual review that uses backend evidence, not clips alone. The ShieldGuard hub [G16] provides release-specific update history. Do not silently convert an announced rollout into independently verified deployment.

Research theme: tactical destruction/visibility, team information and replay evidence. Own fixture can compare scene occlusion and dynamic geometry across viewpoints, then verify that server state and recorded events remain consistent. Unknown exact engine layout and anti-cheat internals.

## 8. PUBG: Battlegrounds

Publisher Nov 5, 2026 developer letter [G10] discusses cheat distribution and ban evasion enforcement; Oct 29 console letter [G11] describes prohibited input devices, latency abuse and single-fire automation concerns. Console platform is not exempt from gameplay and input policy. The case illustrates that an input-only or network-focused threat can exist without access to process memory.

Research theme: large-world relevance, loot semantics, vehicle motion, disconnect handling and input policy. Use a synthetic battle royale scene and local network emulator; evaluate legitimate jitter vs invalid protocol behavior. No claims about proprietary packet format, key material or offsets.

## 9. Overwatch 2

Blizzard's Defense Matrix update [G12] describes cheat software, unapproved peripherals and the broader fair-play program. A hero shooter has diverse movement, ability cooldowns, healing, projectiles and objective mechanics; a single “health” or “team” field is not a universal model. Genre questions: how are ability outcomes validated and replayed? What legitimate hero ability or spectator transition could resemble suspicious behavior? Evaluate using permitted replay material and labeled synthetic events. Private data layouts and detection models are unknown.

## 10. League of Legends

Riot patch 14.9 [G17] documents deployment of Vanguard; 2026 Riot update [G02] covers LoL and VALORANT threat categories and explains different relevance of DMA across the games. This is a useful contrast between a MOBA and a shooter: information advantage depends on how much undisclosed state is client-visible, while scripts/bots can automate decisions from visible state. Study a synthetic fog-of-war and ability simulator; do not automate ranked play. Exact scripting detections and private engine structures are not established.

## 11. Destiny 2

Bungie documents BattlEye as its anti-cheat protection and explains the supported launch requirement [G18]. Research themes: multiplayer vs cooperative activity, account progression and authorized client lifecycle. Do not extrapolate its policies to offline single-player titles or assume the same engine state as another shooter. Public support docs do not disclose a current offset map or enforcement algorithm.

## 12. Battlefield 6

EA documents Javelin integration [G19] and its August 2026 update [G20] discusses TPM enforcement, changed hardware/software policies and allowed accessibility devices. EA's September 4 metrics article [G21] explains revising a match infection metric after high-skill/vehicle player reports affected it. This matters for measurement: changes in reporting, classifier definition or population can change a metric without a corresponding change in actual cheating prevalence. Research theme: high player counts, vehicles, destruction, network events and false-positive validation. Do not compare infection percentages across publishers without common definitions.

## 13. Delta Force

Official G.T.I. Security page [G22] states collaboration with Anti-Cheat Expert (ACE) and describes sanctions for unauthorized software and intentionally teaming with cheaters. The extraction/large-scale FPS case adds inventory, loot, economy and mission progression to tactical state. Own-server fixture: transactions, reconnect, item ownership and match completion; check atomicity and idempotency. The public page does not establish precise live engine layouts, offset values or detection methods. No service/provider recommendations or bypass instructions are included.

## 14. How to extend dossiers to other titles

Add an independent title record: name, platform/mode, version/date, genre, official engine evidence (or unresolved), official anti-cheat/terms, relevant public engineering references, allowed replay/modding interfaces, general threat categories, fixture plan, what is known/unknown, source links and last checked date. For Tarkov, Rust, OSRS or any other game, verify current publisher documentation before stating a security system or implementation. This edition includes the twelve researched entries above; it is not exhaustive of every title. No game-specific binary research performed.
