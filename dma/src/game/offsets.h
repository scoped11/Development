#pragma once
#include <cstdint>

// ============================================================================
// ROB RUN (Unity IL2CPP) — offsets
//
// Dump: 2026-11-14-c01ns-v39.1 (c01ns Il2CppDumper v39.1, metadata v39).
// Synced 2026-11-14 UTC. GameAssembly SizeOfImage 0x8fdc000.
// Steam buildid 26615022. PE SizeOfImage 0x8FDC000. Getters rechecked on the live DLL:
//   Game.get_GameModel                         0x904DA0  [rcx+0xD0]
//   GameModel.get_BattleModel                  0x7EB6B0  [rcx+0x10]
//   BattleModel.get_BattleEntitiesModel        0x7A9F90  [rcx+0x90]
//   BattleModel.get_BattleCameraModel          0x929CA0  [rcx+0x358]
//   Start.UnityEntry.get_Game                  0x7A9000  [rcx+0x28]
//
// Game is not a singleton. The only static on Game.Game is
// IsConnectionKeptOnPause (live PE: `mov al,1; ret`). ContentContext IS a
// singleton (TypeInfo 0x81A4D88 → klass+0xB8 → [0]) but it only holds
// PoolRegistry at +0x10 — not Game. UnityEntry / ViewContainer have no
// *_TypeInfo in script.json (MonoBehaviours). 0x81AD470 is
// UnityEngine.Application, not UnityEntry.
//
// Primitives (BasePrimitiveData<T>.get_Value), value at +0x18:
//   float  0x8331E0  movss xmm0, [rcx+0x18]
//   int    0x7AA140  mov  eax,  [rcx+0x18]
//   object 0x7E6130  mov  rax,  [rcx+0x18]
//   bool   0xA12BD0  movzx eax, byte ptr [rcx+0x18]
//   Vector3 0x97F880 movsd [rdx+0x18], eax [rdx+0x20]
// ============================================================================

namespace offsets {

    // PE SizeOfImage for this Steam build. File size on disk is smaller (~0x8D01800).
    constexpr size_t kDumpGameAssemblySize = 0X8FDC000ULL;
    constexpr size_t kLiveGameAssemblySize = 0X8FDC000ULL;
    // Previous Steam build (this morning, 0x8CFE600) if the game was not restarted.
    constexpr size_t kPriorGameAssemblySize = 0x8CFE600ULL;
    // Older SizeOfImage still seen on some VAD walks.
    constexpr size_t kLegacyGameAssemblySize = 0x8FC9000ULL;

    inline bool IsCurrentGameAssemblySize(size_t imageSize) {
        return imageSize == kDumpGameAssemblySize || imageSize == kLiveGameAssemblySize;
    }
    inline bool IsPriorGameAssemblySize(size_t imageSize) {
        return imageSize == kPriorGameAssemblySize;
    }
    inline bool IsKnownGameAssemblySize(size_t imageSize) {
        return IsCurrentGameAssemblySize(imageSize)
            || IsPriorGameAssemblySize(imageSize)
            || imageSize == kLegacyGameAssemblySize;
    }
    // Named maps can miss GameAssembly after an update. Accept any IL2CPP
    // image in the 128–192MB band so PE discovery still attaches.
    inline bool IsPlausibleGameAssemblySize(size_t imageSize) {
        return IsKnownGameAssemblySize(imageSize)
            || (imageSize >= 0x8000000ULL && imageSize <= 0xC000000ULL);
    }

    namespace unity {
        constexpr uintptr_t ObjectHeader = 0x10;
    }

    namespace il2cpp {
        constexpr uintptr_t Class_Name = 0x10;
        constexpr uintptr_t Class_Namespace = 0x18;
        constexpr uintptr_t Class_StaticFields = 0xB8;

        constexpr uintptr_t String_Length = 0x10;
        constexpr uintptr_t String_Chars = 0x14;

        constexpr uintptr_t Array_Length = 0x18;
        constexpr uintptr_t Array_First = 0x20;

        constexpr uintptr_t List_Items = 0x10;
        constexpr uintptr_t List_Size = 0x18;

        // System.Collections.Generic.Dictionary<TKey,TValue>
        constexpr uintptr_t Dict_Buckets = 0x10;
        constexpr uintptr_t Dict_Entries = 0x18;
        constexpr uintptr_t Dict_Count = 0x20;
        constexpr uintptr_t Dict_FreeList = 0x24;
        constexpr uintptr_t Dict_FreeCount = 0x28;

        // Dictionary.Entry<int, object> — hash, next, key, pad, value
        constexpr uintptr_t Entry_Stride = 0x18;
        constexpr uintptr_t Entry_Hash = 0x00;
        constexpr uintptr_t Entry_Key = 0x08;
        constexpr uintptr_t Entry_Value = 0x10;

        // IPrimitiveData<T> / BasePrimitiveData<T>: serializer at +0x10, value at +0x18
        constexpr uintptr_t Primitive_Value = 0x18;

        // IPrimitiveModel<T> holds IPrimitiveData<T> at +0x10
        constexpr uintptr_t PrimitiveModel_Data = 0x10;
    }

    // Runtime klass = *(GameAssembly + rva). ScriptMetadata *_TypeInfo from script.json.
    // *Dump slots are this morning's GameAssembly (0x8CFE600) if the process was not restarted.
    namespace typeinfo {
        constexpr uintptr_t BattleEntitiesModel = 0x81D46B0;
        constexpr uintptr_t BattleEntitiesModelDump = 0x81D15E8;
        constexpr uintptr_t BattleEntitiesModelUpdater = 0x81D4848;
        constexpr uintptr_t BattleEntitiesModelUpdaterDump = 0x81D1780;
        constexpr uintptr_t BattleEntitiesUpdater = 0x81D4B78;
        constexpr uintptr_t BattleEntitiesUpdaterDump = 0x81D1AB0;
        constexpr uintptr_t CinemachineBrain = 0x8162410;
        constexpr uintptr_t CinemachineCore = 0x8162748;
        constexpr uintptr_t CameraBrainUpdater = 0x81066B0;
        constexpr uintptr_t Cinemachine3rdPersonFollow = 0x8161F40;
        constexpr uintptr_t Game = 0x81455F8;
        constexpr uintptr_t GameDump = 0x8142558;
        constexpr uintptr_t ContentContext = 0x81A4D88;
        constexpr uintptr_t UnityObject = 0x81A0D48;
        constexpr uintptr_t MonoBehaviour = 0x8182E70;
        constexpr uintptr_t GameObject = 0x81468E8;
        // UnityEngine.Application — first lea in UnityEntry.Awake. Not UnityEntry.
        constexpr uintptr_t Application = 0x81AD470;
        // Unnamed TypeInfo slots from UnityEntry.Awake (live klass name-check).
        inline constexpr uintptr_t UnityEntryAwake[] = {
            0x812E130,
            0x812DC70,
            0x8102DC0,
            0x8201C28,
        };
        constexpr uintptr_t GameModel = 0x81465C0;
        constexpr uintptr_t GameModelDump = 0x8143520;
        constexpr uintptr_t BattleModel = 0x81D8010;
        constexpr uintptr_t BattleModelDump = 0x81D4F48;
        constexpr uintptr_t BattleCameraModel = 0x81CB420;
        constexpr uintptr_t BattleCameraModelUpdater = 0x81CB5B8;
    }

    // Game.Battle.BattleEntitiesModel
    namespace battle_entities {
        constexpr uintptr_t Models = 0x50;          // Dictionary<int, IEntityModel>
        constexpr uintptr_t WorldEntities = 0x60;
        constexpr uintptr_t CharacterSlots = 0x80;
        constexpr uintptr_t Player = 0xD0;
        constexpr uintptr_t EntityModels = 0xE0; // ReadOnlyDictionaryNonAlloc<int, IEntityModel>
    }

    namespace battle_entities_updater {
        constexpr uintptr_t BattleEntities = 0x50; // IBattleEntitiesModel
    }

    namespace battle_entities_view_updater {
        constexpr uintptr_t BattleEntities = 0x18; // IBattleEntitiesModel
        constexpr uintptr_t PlayerEntityId = 0x28;
        constexpr uintptr_t BattleModel = 0x70;
        constexpr uintptr_t CharacterViews = 0x110; // Dictionary<int, BattleCharacterEntityView>
        constexpr uintptr_t CarViews = 0x118;       // Dictionary<int, CarEntityView>
    }

    // BattleCharacterEntityView. The mesh the game draws, not the sim model.
    namespace character_entity_view {
        constexpr uintptr_t MovementView = 0x20; // CharacterMovementView
        constexpr uintptr_t FollowCamera = 0x58;
        constexpr uintptr_t LookRotation = 0x80;
        constexpr uintptr_t RendersVisible = 0xB0;
        constexpr uintptr_t CustomizationView = 0xB8;
        constexpr uintptr_t HeadPosition = 0xE8;
    }

    namespace movement_view {
        constexpr uintptr_t Transform = 0x20; // UnityEngine.Transform
    }

    namespace car_entity_view {
        constexpr uintptr_t Transform = 0x28; // UnityEngine.Transform
    }

    // Managed Transform.m_CachedPtr, then the native Transform world position.
    // +0x90 is the UnityPlayer layout noted from the pre-update IDA pass.
    namespace unity_transform {
        constexpr uintptr_t Native = 0x10;
        constexpr uintptr_t WorldPosition = 0x90;
    }

    // UnityPlayer.dll SizeOfImage for 6000.3.18f1. GameObjectManager lives in .data BSS.
    // Confirmed by 10 RIP xrefs in this build (e.g. 0x2F9619 stores the heap GOM here).
    namespace unity_player {
        constexpr uintptr_t GameObjectManager = 0x2230768;
        constexpr size_t kImageSize = 0x2426000ULL;
    }

    inline bool IsKnownUnityPlayerSize(size_t imageSize) {
        return imageSize == unity_player::kImageSize;
    }

    // Unity 6000.3 GameObjectManager is a 0x28-byte heap object (ctor at 0x2F95B0):
    //   +0x00 buckets, +0x08 AND-mask (must be 2^n-1), +0x10 count, +0x16 tag word,
    //   +0x18 embedded circular list sentinel (prev/next, not a pointer to a sentinel).
    // Hash entries are 0x18: hash, tag, list. List first node at +0x8, GO at node+0x10.
    namespace gom {
        constexpr uintptr_t Buckets = 0x00;
        constexpr uintptr_t Count = 0x08;
        constexpr uintptr_t MaskAlt = 0x10;
        constexpr uintptr_t EmbeddedList = 0x18;
        constexpr uintptr_t EntryStride = 0x18;
        constexpr uintptr_t EntryList = 0x10;
        constexpr uintptr_t ListFirst = 0x08;
        constexpr uintptr_t NodeNext = 0x08;
        constexpr uintptr_t NodeObject = 0x10;
        constexpr int EmptyHash = -1;
        constexpr int DeletedHash = -2;
    }

    namespace game_object {
        constexpr uintptr_t Components = 0x20;
        constexpr uintptr_t ComponentCount = 0x30;
        constexpr uintptr_t Name = 0x50;
        constexpr uintptr_t ComponentStride = 0x10;
        constexpr uintptr_t ComponentPointer = 0x08;
        constexpr uintptr_t ManagedHandle = 0x18;
        constexpr uintptr_t ManagedFallback = 0x28;
    }

    namespace unity_entry {
        // MonoBehaviour: m_CachedPtr 0x10, m_CancellationTokenSource 0x18,
        // then ViewContainer 0x20. Live PE: get_Game is `mov rax,[rcx+0x28]; ret`.
        constexpr uintptr_t ViewContainer = 0x20;
        constexpr uintptr_t Game = 0x28;
    }

    namespace game {
        // Plain C# object (not UnityEngine.Object). Live PE: get_GameModel [rcx+0xD0].
        constexpr uintptr_t ViewContainer = 0x10;
        constexpr uintptr_t Scheduler = 0x18;
        constexpr uintptr_t GameModel = 0xD0;
    }

    namespace game_model {
        constexpr uintptr_t BattleModel = 0x10;
    }

    namespace battle_model {
        constexpr uintptr_t BattleEntities = 0x90;
        constexpr uintptr_t ShotInput = 0x110; // Game.Battle.Inputs.InputModel
        constexpr uintptr_t BattleCamera = 0x358;
    }

    namespace battle_camera {
        constexpr uintptr_t Position = 0x10;
        constexpr uintptr_t Orientation = 0x1C;
        constexpr uintptr_t LookAtOrientation = 0x2C;
        constexpr uintptr_t Forward = 0x3C;
        constexpr uintptr_t Right = 0x48;
        constexpr uintptr_t Up = 0x54;
        constexpr uintptr_t DistanceToFollowTarget = 0x60;
        // Packed read from Position through Distance. Updater copies
        // Transform.position/forward/right/up here; Orientation is the
        // camera rotation, LookAtOrientation is that times _flip.
        constexpr uintptr_t PackedBytes = 88;
    }

    // Models.Models.WorldEntities.WorldEntitiesModel.
    // Use the raw dictionaries. These are the same objects that the read-only
    // properties wrap, and this is the path confirmed by the populated live run.
    namespace world {
        constexpr uintptr_t NotSpawnedCharacters = 0x10;
        constexpr uintptr_t BattleCharacters = 0x18;
        constexpr uintptr_t Containers = 0x20;
        constexpr uintptr_t AccessEntities = 0x28;
        constexpr uintptr_t SecurityAlarms = 0x30;
        constexpr uintptr_t Loots = 0x38;
        constexpr uintptr_t VehicleLoots = 0x40;
        constexpr uintptr_t Doors = 0x48;
        constexpr uintptr_t Vehicles = 0x58;
        constexpr uintptr_t Cars = 0x60;
        constexpr uintptr_t Helicopters = 0x68;
        constexpr uintptr_t BreakableColliderSets = 0x70;
        constexpr uintptr_t UnlockEntities = 0x80;
        constexpr uintptr_t BreachEntities = 0x88;
        constexpr uintptr_t StaticLoots = 0x98;
        constexpr uintptr_t RandomLoots = 0xA0;
        constexpr uintptr_t LootNodes = 0xB0;
        constexpr uintptr_t SecurityPanels = 0xB8;
        constexpr uintptr_t Cargos = 0xC8;
        constexpr uintptr_t CollectionCargos = 0xD0;
        constexpr uintptr_t BuildingCargos = 0x100;
        constexpr uintptr_t SafeDoors = 0x110;
        constexpr uintptr_t Storages = 0x128;
        constexpr uintptr_t StorageInventories = 0x130;
        constexpr uintptr_t PlotContainerInventories = 0x138;
        constexpr uintptr_t PlotContainers = 0x140;
        constexpr uintptr_t Shelvings = 0x148;
        constexpr uintptr_t Plots = 0x150;
        constexpr uintptr_t LootInventories = 0x160;
        constexpr uintptr_t ContainerInventories = 0x168;
        constexpr uintptr_t Buildings = 0x190;
        constexpr uintptr_t Grenades = 0x1A0;
        constexpr uintptr_t HideObjects = 0x1B0;
        constexpr uintptr_t PlayerObservers = 0x1C0;
        constexpr uintptr_t FurnitureLoots = 0x220;
        constexpr uintptr_t FurnitureLootInventories = 0x228;
        constexpr uintptr_t BuildingLoots = 0x238;
        constexpr uintptr_t CashLoot = 0x268;
        constexpr uintptr_t DefenseBlockPublic = 0x278;
        constexpr uintptr_t DefenseBlockPrivate = 0x280;
        constexpr uintptr_t LootableObjectGroups = 0x310;
        constexpr uintptr_t CashWithdrawal = 0x460;
    }

    // Models.Models.BattleCharacter.BattleCharacterModel
    namespace character {
        constexpr uintptr_t PrivateModel = 0x10;
        constexpr uintptr_t Id = 0x38;
        constexpr uintptr_t Transform = 0x58;            // TransformModel
        constexpr uintptr_t ReplicatedTransform = 0x60; // CharacterReplicatedTransformModel
        constexpr uintptr_t LookDirection = 0x70;       // LookDirectionModel
        constexpr uintptr_t ReplicatedLook = 0x78;      // ReplicatedLookDirectionModel
        constexpr uintptr_t LastLook = 0x88;            // LastLookDirectionModel
        constexpr uintptr_t Input = 0x98;               // InputModel (BattleInputData)
        constexpr uintptr_t LookInput = 0xB0;           // LookDirectionInputModel
        constexpr uintptr_t Fire = 0xC0;                // FireModel
        constexpr uintptr_t IsDead = 0xF0;              // IPrimitiveModel<bool>
        constexpr uintptr_t ShotArgs = 0xF8;            // ShotArgsModel
        constexpr uintptr_t ShotInterp = 0x108;         // ShotInterpolationModel
        constexpr uintptr_t Skeleton = 0x138;           // SkeletonModel
        constexpr uintptr_t IsSprinting = 0x148;        // IPrimitiveModel<bool>
        constexpr uintptr_t Crouch = 0x160;             // CrouchModel
        constexpr uintptr_t ItemInHands = 0x170;        // ItemInHandsModel
        constexpr uintptr_t HasArmor = 0x1B8;           // IPrimitiveModel<bool>
        constexpr uintptr_t State = 0x1E0;              // BattleCharacterStateModel
        constexpr uintptr_t InVehicle = 0x1C8;          // InVehicleModel
        constexpr uintptr_t Knockout = 0x298;           // KnockoutModel
        constexpr uintptr_t KnockdownLeave = 0x2D8;     // KnockdownLeaveStateModel, IsActive at +0x10
        constexpr uintptr_t Role = 0x208;               // IPrimitiveModel<PlayerRoleDescription>
        constexpr uintptr_t Customization = 0x230;      // ICustomizationModel
        constexpr uintptr_t CopCustomize = 0x2C8;       // CopCustomizeModel
    }

    namespace knockout {
        constexpr uintptr_t Phase = 0x18; // _knockoutPhase IPrimitiveData<KnockoutPhaseDescription>
    }

    namespace crouch {
        constexpr uintptr_t IsCrouching = 0x10; // IPrimitiveData<bool>
    }

    namespace in_vehicle {
        constexpr uintptr_t EntityId = 0x10; // IPrimitiveData<int>, negative when on foot
        constexpr uintptr_t SeatSlot = 0x18; // IPrimitiveData<VehicleSeatSlotDescription>
    }

    namespace enum_item {
        constexpr uintptr_t Index = 0x18;
    }

    // BattleCharacterStateModel._characterHiddenStateModel -> IsHiddenModel (dump).
    namespace character_hidden {
        constexpr uintptr_t IsHidden = 0x10; // bool
    }

    namespace skeleton {
        constexpr uintptr_t Config = 0x10; // SkeletonConfig
        constexpr uintptr_t Bones = 0x18;  // float[]
    }

    namespace skeleton_config {
        constexpr uintptr_t BonesCount = 0x10;
        constexpr uintptr_t BoneDataSize = 0x14;       // floats per bone
        constexpr uintptr_t LocalOrientation = 0x18;   // float index
        constexpr uintptr_t ModelOrientation = 0x1C;   // float index
        constexpr uintptr_t Length = 0x20;             // float index
        constexpr uintptr_t Hierarchy = 0x28;          // int[] children, bone * maxChildren
        constexpr uintptr_t MaxChildren = 0x30;
        constexpr uintptr_t Chain = 0x34;            // root, legs, spine2, spine3, neck, head1, head2
    }

    namespace identified {
        constexpr uintptr_t Id = 0x10; // Il2CppString*
    }

    // TransformModel stores each axis as IPrimitiveData<float>
    namespace transform {
        constexpr uintptr_t X = 0x10;
        constexpr uintptr_t Y = 0x18;
        constexpr uintptr_t Z = 0x20;
    }

    // CharacterReplicatedTransformModel
    namespace replicated {
        constexpr uintptr_t Position = 0x10; // IPrimitiveData<Vector3>
    }

    // LookDirectionModel — Vector3 forward, then yaw and pitch
    namespace look {
        constexpr uintptr_t Forward = 0x10;
        constexpr uintptr_t Yaw = 0x1C;
        constexpr uintptr_t Pitch = 0x20;
        constexpr uintptr_t PackedBytes = 0x14; // Value + Yaw + Pitch
    }

    namespace look_input {
        constexpr uintptr_t X = 0x10;
        constexpr uintptr_t Y = 0x14;
        constexpr uintptr_t Contains = 0x18;
    }

    namespace last_look {
        constexpr uintptr_t Yaw = 0x10;
        constexpr uintptr_t Pitch = 0x14;
        constexpr uintptr_t Direction = 0x18;
    }

    namespace shot_interp {
        constexpr uintptr_t Position = 0x10;
        constexpr uintptr_t LookDirection = 0x1C;
    }

    namespace shot_args {
        constexpr uintptr_t Position = 0x10;  // IPrimitiveData<Vector3>
        constexpr uintptr_t Direction = 0x18; // IPrimitiveData<Vector3>
    }

    // BattleCharacterPrivateModel
    namespace private_model {
        constexpr uintptr_t Health = 0x1B0; // HealthModel
        constexpr uintptr_t Armor = 0x178;  // ArmorModel
        constexpr uintptr_t WeaponSpreadArgs = 0x220; // WeaponSpreadArgsModel
        constexpr uintptr_t WeaponSpread = 0x228;     // IPrimitiveModel<float>
        constexpr uintptr_t CheatSprint = 0x238; // IPrimitiveModel<bool>
    }

    namespace spread_args {
        constexpr uintptr_t HasSpread = 0x10;
        constexpr uintptr_t Spread = 0x14;
        constexpr uintptr_t ShotOffsetX = 0x20;
        constexpr uintptr_t ShotOffsetY = 0x24;
    }

    namespace armor {
        constexpr uintptr_t Amount = 0x20; // IAmmoModel
    }

    namespace ammo_model {
        constexpr uintptr_t Data = 0x10; // IPrimitiveData<int>
    }

    namespace fire {
        constexpr uintptr_t IsFiring = 0x10; // IPrimitiveData<bool>
    }

    namespace shot_input {
        constexpr uintptr_t Triggered = 0x10; // bool
        constexpr uintptr_t FrameCount = 0x14;
        constexpr uintptr_t Mode = 0x18; // Holding=0 Press=1 PressAndHolding=2
    }

    namespace battle_input {
        constexpr uintptr_t Data = 0x10;
        constexpr uintptr_t HasInput = 0x108;
        constexpr uintptr_t IsShot = 0x3C; // Data + IsShotInput 0x2C
    }

    namespace battle_camera_updater {
        constexpr uintptr_t Model = 0x10;
        constexpr uintptr_t Camera = 0x18;
        constexpr uintptr_t Follow = 0x20; // Cinemachine3rdPersonFollow
    }

    namespace customization_view {
        constexpr uintptr_t ColorModel = 0xD0;
        constexpr uintptr_t BodyMaterials = 0x88;
        constexpr uintptr_t HairMaterials = 0x90;
        constexpr uintptr_t BeardMaterials = 0x80;
        constexpr uintptr_t HeadMaterial = 0xA8;
    }

    namespace customization_color {
        constexpr uintptr_t Skin = 0x10;
        constexpr uintptr_t Hair = 0x20;
        constexpr uintptr_t Beard = 0xA0;
        constexpr uintptr_t ColorSize = 0x10; // UnityEngine.Color
    }

    // HealthModel
    namespace health {
        constexpr uintptr_t Amount = 0x10;    // IPrimitiveData<int>
        constexpr uintptr_t IsDead = 0x18;    // IPrimitiveModel<bool>
        constexpr uintptr_t MaxAmount = 0x20; // int
    }

    // Game.Battle.EntityModel.Player.PlayerEntityModel : EntityModel
    namespace player_entity {
        constexpr uintptr_t Id = 0x10;
        constexpr uintptr_t ClientModel = 0x18; // PlayerClientModel
        constexpr uintptr_t ServerModel = 0x20; // BattleCharacterModel
        constexpr uintptr_t PublicId = 0x28;
    }

    // PlayerClientModel : CharacterClientModel. These move with the view,
    // including while the server character list is still empty.
    namespace player_client {
        constexpr uintptr_t SetMovement = 0x30; // SetMovementModel
        constexpr uintptr_t Rotation = 0x38;    // CharacterRotationModel
        constexpr uintptr_t CameraModel = 0x218; // PlayerCameraModel
        constexpr uintptr_t Recoil = 0x2B8;      // RecoilModel
        constexpr uintptr_t FakeSpread = 0x2C0;  // PlayerFakeSpreadModel
    }

    namespace recoil {
        constexpr uintptr_t Offset = 0x10; // Vector2, rewritten every view update
    }

    // PlayerFakeSpreadModel — three floats (dump.cs 0x10 / 0x14 / 0x18), not Vector2.
    namespace fake_spread {
        constexpr uintptr_t Spread = 0x10;
        constexpr uintptr_t Air = 0x14;
        constexpr uintptr_t Move = 0x18;
        constexpr uintptr_t FieldsSize = 0x0C; // Spread + Air + Move
    }

    // Written every view update. This is the follow target the rig
    // starts from, on the character, not the lens.
    namespace player_camera {
        constexpr uintptr_t FollowPosition = 0x10; // Vector3
    }

    // Plain C# updater. +0x10 is the CinemachineBrain it drives.
    namespace camera_brain_updater {
        constexpr uintptr_t CameraBrain = 0x10;
    }

    // CinemachineCore.sInstance is static-fields+0x8. mActiveBrains is the
    // List<CinemachineBrain> at instance+0x10. Use that instead of a heap scan.
    namespace cinemachine_core {
        constexpr uintptr_t Instance = 0x8;
        constexpr uintptr_t ActiveBrains = 0x10;
    }

    // CinemachineBrain.CurrentCameraState is a CameraState struct at +0xB0.
    // W2S uses CorrectedPosition = RawPosition + PositionCorrection and
    // CorrectedOrientation = RawOrientation * OrientationCorrection.
    // RawOrientation alone is the vcam pose (no look pitch) — that parked
    // ESP in the sky. The Brain on the Unity Camera is OutputCamera +0x50.
    namespace cinemachine_brain {
        constexpr uintptr_t CachedPtr = 0x10;
        constexpr uintptr_t UpdateMethod = 0x30;
        constexpr uintptr_t OutputCamera = 0x50;
        constexpr uintptr_t LastFrame = 0x78;
        constexpr uintptr_t CurrentCameraState = 0xB0;
        constexpr uintptr_t FieldOfView = 0xB0;
        constexpr uintptr_t RawPosition = 0xFC;
        constexpr uintptr_t RawOrientation = 0x108;
        constexpr uintptr_t RawPose = 0xFC;
        constexpr uintptr_t PositionCorrection = 0x128;
        constexpr uintptr_t OrientationCorrection = 0x134;
    }

    // Cinemachine3rdPersonFollow, and the aiming subclass that extends it.
    // ShoulderOffset is in follow-target local space. CameraSide 0 is the
    // left shoulder and 1 is the right. The lens sits CameraDistance behind
    // the hand. The aiming subclass blends the same fields and stores the
    // distance it actually used at +0xD0.
    namespace third_person {
        constexpr uintptr_t ShoulderOffset = 0x34;          // Vector3
        constexpr uintptr_t VerticalArmLength = 0x40;
        constexpr uintptr_t CameraSide = 0x44;
        constexpr uintptr_t CameraDistance = 0x48;
        constexpr uintptr_t ShoulderOffsetAimed3 = 0x84;    // Vector3
        constexpr uintptr_t VerticalArmLengthAimed3 = 0x90;
        constexpr uintptr_t CameraDistanceAimed3 = 0x94;
        constexpr uintptr_t ShoulderOffsetAimed1 = 0x98;    // Vector3
        constexpr uintptr_t VerticalArmLengthAimed1 = 0xA4;
        constexpr uintptr_t CameraDistanceAimed1 = 0xA8;
        constexpr uintptr_t WeightAiming = 0xAC;
        constexpr uintptr_t WeightAimMode = 0xB0;
        constexpr uintptr_t CameraDistanceLive = 0xD0;
        constexpr uintptr_t BlockEnd = 0xD4;                // one past the live distance
    }

    namespace set_movement {
        constexpr uintptr_t Position = 0x10; // float X, Y, Z at 0x10/0x14/0x18
    }

    namespace client_rotation {
        constexpr uintptr_t LookRotation = 0x10; // Quaternion
        constexpr uintptr_t LookYaw = 0x20;
        constexpr uintptr_t Yaw = 0x24;
        constexpr uintptr_t Pitch = 0x28;
    }

    // ClientCharacterSlotsModel
    namespace slots {
        constexpr uintptr_t ByEntityId = 0x18; // Dictionary<int, BattleCharacterSlotModel>
    }

    // BattleCharacterSlotModel -> BattleCharacterSlotData
    namespace slot {
        constexpr uintptr_t Data = 0x10;
    }

    namespace slot_data {
        constexpr uintptr_t PublicId = 0x28; // IPrimitiveData<long>
        constexpr uintptr_t Nickname = 0x30; // IPrimitiveData<string>
        constexpr uintptr_t ClanTag = 0x38;  // IPrimitiveData<string>
    }

    // VehicleModel
    namespace vehicle {
        constexpr uintptr_t Id = 0x10;
        constexpr uintptr_t Transform = 0x28; // VehicleTransformModel
        constexpr uintptr_t Type = 0x118;     // VehicleTypeDescription
        constexpr uintptr_t BoundsCenter = 0xA8; // Vector3, relative to the centre of mass
        constexpr uintptr_t Bounds = 0xB4;       // Vector3 full size
        constexpr uintptr_t InertiaSize = 0xC0;  // Vector3 body size, tighter than the bounds
        constexpr uintptr_t CenterFromPivot = 0x14C; // vertical centre relative to the pivot
    }

    namespace cash {
        constexpr uintptr_t Amount = 0x10;       // IPrimitiveData<int>
        constexpr uintptr_t Description = 0x18;  // LocationCashLootNodeDescription
    }

    namespace cash_desc {
        constexpr uintptr_t Position = 0x24; // Vector3, placed with the location
    }

    namespace vehicle_transform {
        constexpr uintptr_t Position = 0x10;     // IPrimitiveData<Vector3>, replicated
        constexpr uintptr_t PivotPosition = 0x54; // live Vector3, written by physics
        constexpr uintptr_t PivotOrientation = 0x60; // live Vector4
    }

    namespace car {
        constexpr uintptr_t Id = 0x10;
        constexpr uintptr_t Transform = 0xC0; // VehicleTransformModel
    }

    namespace helicopter {
        constexpr uintptr_t Id = 0x10;
        constexpr uintptr_t Transform = 0x70; // VehicleTransformModel
    }

    // LootModel : ILootModel
    namespace loot {
        constexpr uintptr_t Id = 0x10;
        constexpr uintptr_t Transform = 0x20; // ITransformModel
        constexpr uintptr_t Bag = 0x30;       // IPrimitiveModel<CharacterBagDescription>
        constexpr uintptr_t Role = 0x38;      // IPrimitiveModel<PlayerRoleDescription>
    }

    // LootEntityInventoryModel. Name is the player who dropped the bag.
    namespace loot_inventory {
        constexpr uintptr_t Id = 0x10;
        constexpr uintptr_t Name = 0x18; // IPrimitiveModel<string>
    }

    // RandomLootObjectModel. The item pointer is a description, not a bag index.
    namespace random_loot {
        constexpr uintptr_t IsTaken = 0x10;   // IPrimitiveData<bool>
        constexpr uintptr_t Item = 0x18;      // IPrimitiveData<LootObjectItemDescription>
        constexpr uintptr_t Description = 0x20;
    }

    namespace random_loot_desc {
        constexpr uintptr_t Position = 0x24; // Vector3
    }

    namespace static_loot {
        constexpr uintptr_t IsTaken = 0x10;   // IPrimitiveData<bool>
        constexpr uintptr_t Description = 0x18;
    }

    namespace loot_item {
        constexpr uintptr_t ItemDescriptionFunc = 0x20; // Func<InventoryItemDescription>
    }

    namespace inventory_item {
        constexpr uintptr_t Type = 0x20;         // InventoryItemType (byte)
        constexpr uintptr_t Rarity = 0x30;       // RarityItem
        constexpr uintptr_t WeaponFunc = 0x50;
        constexpr uintptr_t BasePriceFunc = 0x78; // Func<int>
        constexpr uintptr_t DecoderFunc = 0x180;
    }

    namespace il2cpp_delegate {
        constexpr uintptr_t Target = 0x20;
    }

    namespace static_loot_desc {
        constexpr uintptr_t ItemFunc = 0x28; // Func<LootObjectItemDescription>
        constexpr uintptr_t Position = 0x30; // Vector3
    }

    namespace container {
        constexpr uintptr_t Description = 0x18; // LocationContainerDescription
        constexpr uintptr_t Position = 0x28;    // PositionModel, floats at +0x10
        constexpr uintptr_t UnlockState = 0x30;
        constexpr uintptr_t IsEmpty = 0x38;     // IPrimitiveModel<bool>
        constexpr uintptr_t DrillEndTs = 0x40;  // IPrimitiveModel<long>
    }

    namespace location_container {
        constexpr uintptr_t InventoryEntityId = 0x24;
        constexpr uintptr_t ContainerDescriptionFunc = 0x28;
        constexpr uintptr_t Position = 0x30;
        constexpr uintptr_t IsCashWithdrawal = 0x79;
    }

    namespace container_desc {
        constexpr uintptr_t TypeFunc = 0x20;
        constexpr uintptr_t DrillFunc = 0x28;
    }

    namespace container_inventory {
        constexpr uintptr_t Id = 0x10;
        constexpr uintptr_t Inventory = 0x18;
    }

    namespace inventory_model {
        constexpr uintptr_t Slots = 0x18;
    }

    namespace inventory_slots {
        constexpr uintptr_t Models = 0x10;
    }

    namespace inventory_slot {
        constexpr uintptr_t ItemModel = 0x18;
    }

    namespace inventory_slot_item {
        constexpr uintptr_t Data = 0x10;
    }

    namespace hide_object {
        constexpr uintptr_t OccupantId = 0x10; // -1 empty
        constexpr uintptr_t Description = 0x18;
    }

    namespace hide_desc {
        constexpr uintptr_t Position = 0x30;
        constexpr uintptr_t Orientation = 0x3C;
        constexpr uintptr_t HideObjectFunc = 0x28;
    }

    namespace plot {
        constexpr uintptr_t Data = 0x10;
        constexpr uintptr_t Id = 0x18;
        constexpr uintptr_t Slot = 0x20;
        constexpr uintptr_t Description = 0x28;
        constexpr uintptr_t MainBuilding = 0x58;
        constexpr uintptr_t MainBuildingLoot = 0x80;
        constexpr uintptr_t ClanBonus = 0xA8;
        constexpr uintptr_t Vip = 0xB0;
    }

    namespace customization {
        constexpr uintptr_t Data = 0x10;
    }

    namespace customization_data {
        constexpr uintptr_t Sex = 0x20;
        constexpr uintptr_t Beard = 0x28;
        constexpr uintptr_t Body = 0x30;
        constexpr uintptr_t Hair = 0x38;
        constexpr uintptr_t SkinColor = 0x70;
        constexpr uintptr_t Bag = 0x78;
        constexpr uintptr_t FBody = 0xB0;
        constexpr uintptr_t FHair = 0xB8;
        constexpr uintptr_t FSkinColor = 0xE0;
        constexpr uintptr_t FBag = 0xE8;
    }

    namespace cop_customize {
        constexpr uintptr_t Data = 0x10;
    }

    namespace main_building {
        constexpr uintptr_t Storages = 0x40;
        constexpr uintptr_t Shelvings = 0x50;
        constexpr uintptr_t Doors = 0x80;
        constexpr uintptr_t PlotContainers = 0x90;
        constexpr uintptr_t DefenseBlock = 0xA8;
    }

    namespace defense_public {
        constexpr uintptr_t Description = 0x10;
        constexpr uintptr_t RaidLockTs = 0x18;
        constexpr uintptr_t IsRaided = 0x20;
        constexpr uintptr_t IsSlotsEmpty = 0x28;
    }

    namespace defense_private {
        constexpr uintptr_t Id = 0x10;
        constexpr uintptr_t BaseDefensePoints = 0x14;
        constexpr uintptr_t DefensePoints = 0x28;
    }

    namespace location_defense {
        constexpr uintptr_t EntityIds = 0x20;
        constexpr uintptr_t Position = 0x28;
    }

    namespace defense_ids {
        constexpr uintptr_t PublicId = 0x10;
        constexpr uintptr_t PrivateId = 0x14;
    }

    namespace description_model {
        constexpr uintptr_t Value = 0x10;
    }

    namespace builder_door {
        constexpr uintptr_t DoorModel = 0x28;
    }

    namespace door_model {
        constexpr uintptr_t State = 0x30;
    }

    namespace door_state {
        constexpr uintptr_t Value = 0x10;
    }

    namespace plot_data {
        constexpr uintptr_t OwnerPublicId = 0x28; // IPrimitiveData<long>
        constexpr uintptr_t VipEndTs = 0x38;
    }

    namespace plot_desc {
        constexpr uintptr_t Position = 0x10;
        constexpr uintptr_t Orientation = 0x1C;
    }

    namespace plot_container {
        constexpr uintptr_t Description = 0x10;
        constexpr uintptr_t Position = 0x38;
        constexpr uintptr_t IsEmpty = 0x40;
        constexpr uintptr_t Breach = 0x48;
    }

    namespace shelving {
        constexpr uintptr_t Description = 0x18;
        constexpr uintptr_t CargoInventory = 0x20;
        constexpr uintptr_t Breach = 0x40;
    }

    namespace cargo {
        constexpr uintptr_t Id = 0x18;
        constexpr uintptr_t Description = 0x28;
        constexpr uintptr_t Transform = 0x30;
    }

    namespace building {
        constexpr uintptr_t Id = 0x38;
        constexpr uintptr_t Description = 0x40;
        constexpr uintptr_t Cameras = 0x70;
        constexpr uintptr_t SafeDoor = 0xB8;
        constexpr uintptr_t SecurityAlarm = 0xC8;
    }

    namespace building_desc {
        constexpr uintptr_t Position = 0x30;
        constexpr uintptr_t Cameras = 0xC0;
        constexpr uintptr_t HideObjects = 0xC8;
        constexpr uintptr_t TypeFunc = 0x28;
    }

    namespace cameras_model {
        constexpr uintptr_t Descriptions = 0x10;
        constexpr uintptr_t Models = 0x18;
    }

    namespace camera_model {
        constexpr uintptr_t IsBroken = 0x10; // IPrimitiveData<bool>
        constexpr uintptr_t Description = 0x28;
    }

    namespace location_camera {
        constexpr uintptr_t Position = 0x30;
        constexpr uintptr_t Orientation = 0x3C;
    }

    namespace safe_door {
        constexpr uintptr_t IsUnlocked = 0x10;
        constexpr uintptr_t UnlockEndTs = 0x18;
        constexpr uintptr_t Description = 0x28;
    }

    namespace furniture_loot {
        constexpr uintptr_t Position = 0x18;
        constexpr uintptr_t HasCargo = 0x30;
        constexpr uintptr_t FurnitureType = 0x48;
        constexpr uintptr_t FurnitureId = 0x50;
    }

    namespace loot_node {
        constexpr uintptr_t Description = 0x10;
        constexpr uintptr_t IsEmpty = 0x30;
    }

    namespace loot_node_desc {
        constexpr uintptr_t LootFunc = 0x30;
        constexpr uintptr_t Position = 0x38;
    }

    namespace cash_withdrawal {
        constexpr uintptr_t ContainerEntityId = 0x18;
        constexpr uintptr_t InteractPoint = 0x1C;
    }

    namespace grenade_desc {
        constexpr uintptr_t InstantDamage = 0x30;
        constexpr uintptr_t Radius = 0x38;
        constexpr uintptr_t InstantBlind = 0x3C;
        constexpr uintptr_t Smoke = 0x3D;
    }

    namespace list {
        constexpr uintptr_t Items = 0x10;
        constexpr uintptr_t Size = 0x18;
    }

    namespace clan_bonus {
        constexpr uintptr_t Tag = 0x10;
    }

    namespace grenade {
        constexpr uintptr_t Description = 0x18; // GrenadeDescription
        constexpr uintptr_t Transform = 0x30;   // GrenadeTransformModel
    }

    namespace grenade_transform {
        constexpr uintptr_t Position = 0x18; // Vector3
    }
}
