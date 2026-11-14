# Gameplay manipulation and information advantage: technical taxonomy

Scope: cross-title threat modeling, offline replay analysis, and game-development security. Common cheat terms are not technical standards. Sources G02–G13, E03, E06–E07. This describes information requirements and effects, not live competitive cheat implementations.

## 1. Classification

Three independent questions: what information is available, what action is automated or modified, and which component decides the authoritative outcome? A feature can be built around pixels, client data, approved telemetry or an input device. The name does not establish the mechanism or whether it works. An authorized spectator replay may contain data that must not be present in a live player view.

| Family / terms | Data dependencies | Effect / trust boundary | Authorized research analogue |
|---|---|---|---|
| ESP, wallhack, health/name/distance | Entity identity, state, coordinate system, visibility | Reveals information outside intended gameplay | Own-engine debug view; authorized replay |
| Radar/minimap augmentation | Positions, map scale, knowledge rules | Location information advantage | Replay heatmap, map editor |
| Chams / altered visibility | Materials, depth behavior, membership | Removes intended visual occlusion | Debug materials, culling visualization |
| Aimbot, triggerbot, pixelbot | Observations, input timing | Automates actions | Offline detection evaluation, approved accessibility QA |
| Recoil/spread manipulation | Input and weapon behavior | Reduces intended weapon constraints | Owned weapon simulator |
| Silent aim / hit manipulation | Shot representation, outcome validation | Attempts to separate displayed aim from outcome | Server consistency checks |
| Speed, fly, teleport, noclip | Physics, movement and authority | Invalid state transitions | Own-game movement validation |
| Inventory, loot, economy abuse | Items, transactions, ownership | Unauthorized information or state changes | Synthetic inventory ledger |
| Bots, macros, farming | Task state and events | Automated progression | NPC AI, QA harness |
| Lag switching, replay abuse | Network timing, message identity | Disrupts synchronization or repeats operation | Local network emulator and idempotency test |
| Boosting, account abuse | Ranking, identity and matchmaking | Abuses progression and trust | Synthetic fraud analytics |

Riot [G02,G03], Activision [G05], Ubisoft [G09] and PUBG [G11] document several of these categories. Their policies differ. Do not assume an accessibility device is prohibited merely because it is a device; consult the relevant publisher's policy and approved accessibility options.

## 2. Information features: semantics and failure modes

A position requires an identity, units, coordinate frame, validity time and source. An entity recorded in a replay may not be a live client object. A stale actor can remain after leaving view. Distance may be straight-line, path length or screen-space separation. Health might be authoritative, predicted, normalized, or a UI-only value. Team could mean temporary party, faction or spectator grouping. A missing field is not zero. Use known labels in a fixture, and distinguish actual state from display state. Visibility is not merely whether a pixel is drawn: it may mean network relevance, geometric line of sight, occlusion, game rules, or a previous observation. Riot's public fog-of-war design is a useful example of withholding information at the network boundary [G13].

An offline radar heatmap needs a map transform, bounds, documented origin and axes, and a clear viewer role. An offline diagnostic view can color an owned scene's objects by state, but material or depth changes have no necessary relationship to network authority. A camera position and a character position may be sampled at different times. See 05 for projection validation and 06 for relevance.

## 3. Automation and state modification

Aim automation can consume visible pixels or semantic state; the source affects observability and limitations, not policy. “Silent aim” is a marketing term, not a universal capability. Local camera, weapon spread, recoil, animation, projectile simulation and server validation can be separate. A visual modification does not imply a different outcome on the server. A vision classifier can be evaluated offline without creating gameplay inputs. Evaluate confusion matrices across maps, UI themes, occlusion, lighting and session splits. Do not use visual classifier performance as proof of cheating prevalence.

Movement state may be client-predicted while authoritative collision and acceptance occur elsewhere. Test legitimate invariants (finite positions, legal movement, correct collision) in your own server. Account economy needs authorization, atomicity and idempotency; a displayed quantity is not an entitlement. Test two simultaneous requests for the last item and ensure only one succeeds. Replaying a valid transaction must not duplicate a reward. These are server-design concerns, not guaranteed abilities of any cheat architecture.

## 4. Detailed offline inputs and output contracts

```json
{"schema":2,"match":"synthetic-001","tick_rate":20,"coordinate_system":"right-handed,meter","viewer":"spectator","records":[{"tick":100,"entity_id":"actor-7","generation":1,"position":[2,0,8],"visibility_to_viewer":"visible","team":"blue","health_fraction":0.75}]}
```

Validation: unique ID+generation, monotonic tick, finite position, normalized health in [0,1], explicit unknown state when unavailable, provenance for each field. Separate train/test by session, not adjacent frames. Viewer role is required because a replay may expose more information than a live participant. Include dataset license, hashes, label definitions and labeler agreement.

| Evaluation | Input | Expected output | Metric / rejection condition |
|---|---|---|---|
| Entity association | Crossing synthetic actors | Same-ID tracks | identity switches, missing samples |
| Map transform | Known positions and map bounds | Replay coordinates | error in pixels, out-of-bounds handling |
| Visual classifier | Labeled owned-scene screenshots | Class/bounds | precision, recall, IoU |
| Visibility oracle | Known camera and scene | visible/hidden/unknown | agreement with fixture truth |
| Movement rules | Own server command trace | accepted/rejected | invariants, false rejects |
| Inventory ledger | Synthetic requests and retries | ledger and responses | conservation, no duplicate reward |
| Replay alignment | Jitter and missing samples | timestamped playback | monotonicity, age indicators |

IoU is intersection area divided by union area. Precision = TP/(TP+FP); recall = TP/(TP+FN). Define the class and matching rule, and report both measures: a system that only finds easy examples may have high precision and low recall. For synthetic trajectory crossing, avoid associating by proximity alone when a stable source ID exists.

## 5. Genre-specific threat modeling

Tactical FPS: hidden enemy information and timing, low time-to-kill, weapon outcome. Battle royale/extraction: long-range world, loot, inventory, traversal, matchmaking and reconnect. Hero shooter: abilities, healing, cooldown, projectile and objective state. MOBA: fog, resources, abilities, automated decisions. MMO: farming, trades, currency and persistent account state. Racing/sports: physics and control invariants, leaderboard integrity. Sandbox/co-op: supported mods may be allowed in one context and disallowed on competitive servers. These are general genre risks, not claims about a named game's memory layout.

## 6. Feature evidence record

Record common term, observed behavior, title/build, documented policy, information source, effect, authority boundary, legitimate variation, alternative explanations, authorized fixture, expected results, source IDs and unresolved questions. Separate reported claims from reproducible evidence. No live target feature or offset was tested.
