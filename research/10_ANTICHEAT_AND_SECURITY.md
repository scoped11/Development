# Anti-cheat and game security: defensive architecture, evidence and limits

Sources H01,H02,H07,G02–G22. Research for understanding protections and fair-play engineering. No detection-evasion instructions, bypass recipes, spoofing guidance or protected-game testing.

## 1. Defense in depth

Security goals include protecting information, ensuring valid state transitions, preserving integrity of the executable environment, resisting automation, maintaining account trust and providing fair enforcement. No single sensor can establish every threat. A secure server, minimized replication, client/platform integrity, behavioral analytics and review can complement each other. A detected violation is a policy and evidence question, not a property of one memory architecture.

| Layer | Objective | Typical public mechanism category | Important limitation |
|---|---|---|---|
| Firmware / boot | establish trusted startup | Secure Boot, measured boot | not a complete runtime guarantee |
| Device memory isolation | restrict DMA | IOMMU/remapping | depends on platform and policy |
| OS | isolate processes and enforce privileges | handles, access rights, driver signing | legitimate applications can still contain defects |
| Application | integrity and allowed interfaces | signatures, approved plugins, validation | cannot replace server-side rules |
| Network | minimize information and validate commands | relevance, authority, replay protection | latency and complex gameplay need careful handling |
| Behavior | detect prohibited patterns | telemetry/analytics/review | false positives, population change |
| Account | protect progression/identity | authentication, sanctions, appeals | shared devices/accounts complicate attribution |

## 2. Secure Boot, TPM and IOMMU are different controls

Secure Boot verifies a chain of trusted boot components according to firmware policy. TPM can support measured boot and protected key/attestation operations. IOMMU translates and restricts device memory access. They are complementary, not substitutes. Windows Kernel DMA Protection documents use of IOMMU for isolation and OEM requirements for boot-time protection [H01,H07]. A TPM alone does not restrict a peripheral's memory transactions. A boot check does not prove that all runtime applications are trustworthy. An IOMMU does not prove that a game server never discloses hidden information.

Epic's Fortnite tournament announcement [G06] names all three features. Battlefield 6's policy describes Secure Boot and TPM requirements in official updates [G19,G20,G23]. Record scope (platform, mode, date) rather than generalizing them to every title. For a lab keep protections enabled, verify configuration through documented OS/vendor reporting, and use driver-authorized buffers or offline sources. If a configuration is unsupported, record the limitation instead of weakening protections.

## 3. Client-side and kernel security

User-mode process isolation protects address spaces through OS mechanisms. A kernel component may have higher privileges and visibility, but increases the system's security and stability burden. Legitimate signed drivers should expose minimal capabilities, validate all inputs, restrict handles and device access, and maintain a clear update/revocation process. An unauthorized component's presence is not automatically evidence that every observed gameplay anomaly came from it.

RICOCHET's publisher description [G04] explicitly discusses its PC kernel driver alongside backend analytics; Riot's [G02] discusses multiple vectors including DMA and vision. These are high-level public descriptions, not full detection implementations. Do not assume that a separate computer, device or process makes a feature invisible, nor that every game observes the same signals.

## 4. Server-side defenses and telemetry

Design command validation from the rules of the game. A local object value or visual event cannot override server authority by itself. Validate ownership, sequencing, finite numbers, state, resource consumption, collision and timing as applicable. Reduce disclosure by sending only authorized data. Riot fog-of-war design [G13] illustrates information minimization as a response to wallhacks. See 06 for detailed synthetic network cases.

### Telemetry research schema

```json
{"event":"synthetic-command-validation","server_tick":512,"session":"lab-17","actor":"test-2","generation":1,"command_seq":88,"result":"rejected","reason":"invalid-owner","schema":1,"source":"owned-test-server"}
```

Data minimization: collect only what is needed, document consent/authorization, retention and access. Avoid publishing private player identifiers or sensitive device information. Retain sufficient context for review and appeal; do not treat a bare score as definitive evidence. Store classifier/version and evidence source when evaluating detection models, not private threshold values from protected systems.

## 5. Measurement quality and false positives

Confusion matrix: true positives, false positives, true negatives, false negatives against labeled ground truth. Precision = TP/(TP+FP); recall = TP/(TP+FN); false positive rate = FP/(FP+TN). In low-prevalence populations, even a small false positive rate can dominate positive results. Example synthetic 100,000 observations: 1,000 known positives, 99,000 negatives, recall 90%, false positive rate 1%. TP=900, FP=990, precision≈47.6%. This is an illustrative base-rate exercise, not a game detection statistic.

Reported bans are not detection probabilities. Match infection rate, account ban count, hardware sanctions and player reports measure different things. Denominators, time window, classifier change, mode and region are critical. EA's Battlefield 6 July metrics article [G21] discusses changing a metric because reports of legitimate high-skill and vehicle players affected it. Use the original and revised definitions when comparing periods. EA's Apex September report [G15] is a snapshot of enforcement actions, not a population prevalence measure. Do not compare publishers' values without compatible definitions.

### Review and legitimate variation

A suspicious recording can be affected by spectator interpolation, network latency, replay error, high skill, game mechanics, accessibility hardware and ordinary bugs. Ubisoft describes backend evidence in manual review [G09]. A defensible process separates automated flags, evidence collection, human assessment and sanctions/appeals. For a synthetic evaluator, split evaluation by game mode/session, measure error rates and test high-skill/lag outliers. No live player is classified here.

## 6. Input devices, computer vision and policy

A peripheral can automate input without reading RAM. A vision system may classify visible pixels without accessing process objects. Both can still violate policies. EA Apex [G07], Battlefield [G20], PUBG [G11], and Riot [G02,G03] provide public examples. Accessibility is not the same as unauthorized automation; check publisher-supported options and avoid assuming a general-purpose tool is approved in every title. No architecture guarantees account safety.

## 7. Research controls and reporting

Record source type: official platform/standard, publisher announcement, publisher policy, academic research or project documentation. Treat vendor claims about effectiveness as reported, not independent evidence. Keep publication dates and exact scope. For unknown protection internals, write unresolved. Test only owned applications with synthetic data; do not launch protected titles to validate detection or use live public matches for experiments. No circumvention, live anti-cheat analysis or enforcement testing occurred.
