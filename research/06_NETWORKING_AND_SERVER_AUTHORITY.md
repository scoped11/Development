# Multiplayer networking, server authority and game security boundaries

Sources E03,E04,E06,E07,G13,G04,G09,G11. Engine documentation describes supported designs, not a guarantee about a commercial game's implementation.

## 1. Simulation roles and authority

A dedicated server has no local player; a listen server may host a client. Peer or distributed authority models assign ownership differently. Unity documents server and distributed authority [E06,E07]; Unreal documents authoritative server replication [E03]. Authority should be evaluated per object, property and action, not simply per application.

| Data | Possible owner | Client representation | Security invariant |
|---|---|---|---|
| Position | Server simulation | predicted/interpolated pose | valid movement |
| Health | Server damage system | replicated/UI | validated damage only |
| Inventory | Server/account service | cache | atomic authorized updates |
| Camera | Local presentation | immediate pose | not proof of shot acceptance |
| Rank | Backend | displayed result | validated match/account association |
| Hidden opponent | Server relevance | possibly absent | permitted disclosure only |

An authoritative server can still disclose too much data or accept invalid requests. Conversely, memory or input access does not confer control over server-only state. Riot's fog-of-war design is a documented visibility security example [G13].

## 2. Transport and temporal model

Tick rate is simulation frequency; render frame rate and send rate are separate. RTT measures round-trip time; exact one-way delay requires clock/path information. Jitter, loss, reorder and duplication are different. Bandwidth is capacity; goodput is useful application bytes per time. Reliable ordered transport can delay subsequent messages behind a loss; unreliable updates may suit replaceable data if protocol semantics handle loss. No universal TCP/UDP rule applies to every game event.

Prediction displays local action effects before receiving authority. A server validates a sequenced command, simulates it and acknowledges. Client reconciliation starts from the authoritative state and applies still-pending valid inputs according to its design. Smoothing may conceal a visual correction but does not determine the authoritative outcome. A stale acknowledgement must not override a newer one.

Illustrative owned-server trace:

| Event | Sequence/tick | State |
|---|---|---|
| Server baseline | tick 50 | x=10.0 |
| Client prediction | command 101 | x=11.0 |
| Server simulation | tick 52, command 101 | x=10.8 |
| Ack | tick 52 | x=10.8 |
| Client reconciliation | tick 53 | authoritative base 10.8 + valid pending inputs |

The discrepancy is not proof of cheating; collision, rounding or prediction can explain it. Log input sequence, acknowledgement, tick and timestamp. In a controlled fixture test stale ack, duplicate input, reconnect and loss.

## 3. Replication, delta baselines and interest management

Server may select relevant objects per connection, schedule updates and send deltas relative to a baseline. If the baseline is unavailable, a delta can be invalid. Use explicit schema and baseline identifiers and a documented resynchronization mechanism. Ownership, dormancy, relevance and priority influence which state arrives and when. Unreal documents multiple replication systems [E03]; Unity's packages offer different object/entity designs [E06]. Do not assume complete client state.

### Visibility contract for an owned project

Define roles: two players and an authorized spectator. Players receive public scoreboard, their own inventory, and other actors only when a visibility oracle permits. Spectator receives a separate authorized replay stream. Log serialized outgoing records by role and assert hidden private data never appears in player messages. Exercise visibility transition, disconnect/reconnect and stale packet. Previous observations may remain legitimate last-known state: distinguish current, last-known and absent. No live hidden-state acquisition is necessary.

## 4. Application-level invariants

Authenticate requests as appropriate. Validate type, bounds, identity, ownership, resource constraints, server-side timing and current state. Avoid client timestamps as sole evidence. A valid message is not necessarily an authorized action. Ordinary lag, accessibility tools and device variation require careful interpretation.

| Invariant | Synthetic negative test | Expected |
|---|---|---|
| Ownership | Actor A tries to mutate B | reject and audit |
| Finite values | NaN position | reject, no state corruption |
| Idempotency | duplicate transaction ID | prior result or reject, no extra reward |
| Lifecycle | action for destroyed generation | reject stale identity |
| Inventory | two requests consume last item | one success, one conflict |
| Permissions | player requests spectator stream | no disclosure |
| Protocol | oversized length / unknown enum | bounded error |

Keep server authoritative outcome and context in event logs. Replays may be derived from snapshots, inputs or event logs and omit intermediate states. Review viewpoint, replay version, interpolation and network timing. Ubisoft states manual decisions use backend evidence as well as player clips [G09]. A suspicious visual clip alone cannot establish a mechanism.

## 5. Lab input/output plan

Use owned local server, two test clients, fixed seed, explicit build, synthetic accounts, no production traffic. Conditions: baseline, 80 ms added RTT, 1% loss, reorder/duplicate case, disconnect. Use a controlled emulator, record its limits. Output trace fields: monotonic timestamp, role, connection ID, sequence, message type, byte length, accept/reject, authoritative state hash. Evaluate p95 ack delay, resync count, invalid-command rejection and false rejection of valid commands. Test no public endpoints. No networking experiments executed here.
