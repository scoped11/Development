# Memory, executable formats, address semantics and binary layout

Research reference for authorized offline analysis. See [13_SOURCE_CATALOG.md](13_SOURCE_CATALOG.md), especially M01–M07, H02, R01–R06.

## 1. The vocabulary that must be correct

| Term | Meaning | Common confusion |
|---|---|---|
| File offset | Byte index in original on-disk file | Not necessarily equal to RVA |
| RVA | Offset from loaded image base | Not an object field displacement |
| Preferred VA | Preferred image base + RVA | Not necessarily actual runtime VA |
| Runtime VA | Virtual address in a particular process/session | Changes with mappings and ASLR |
| Field displacement | Offset from a particular object/subobject | Not a global address |
| Physical address | CPU physical memory coordinate | Not generally equivalent to device bus address |
| IOVA | Device-visible address under IO translation | Not process VA |
| Handle | Identifier interpreted by an owner | Not necessarily a pointer |
| Index | Element position in a specified container | Requires stride/layout and bounds |

Windows gives each process its own virtual address space [M01]. A numerical value may be meaningful in multiple spaces simultaneously. Include the address kind in every record rather than relying on magnitude. Pointer width is an ABI property; a 64-bit executable can still have 32-bit RVAs, offsets, indices and serialized identifiers.

## 2. PE structure, sections and relocations

The PE/COFF specification [M06] defines DOS header, PE signature, COFF header, optional header, section table and data directories. Parse the actual declared sizes rather than assuming fixed offsets beyond documented structures. An image can have distinct raw and virtual sizes; the loader may zero-fill a virtual tail. The certificate table uses a file location instead of an ordinary mapped RVA. The optional header magic distinguishes PE32 and PE32+; image machine and magic must be consistent with the intended tool.

For an ordinary file-backed section, `raw_offset = PointerToRawData + (rva - VirtualAddress)` only if the RVA lands in that section AND the requested range has raw backing. Check bounds against the actual file. An RVA inside headers requires header-aware mapping. An address in a virtual zero-fill region has no corresponding stored raw bytes. The section alignment is not the file alignment. Relocations can adjust stored addresses when the loader chooses another base; imported addresses and runtime-generated state can differ from file contents.

### Worked synthetic mapping

Given image base `0x140000000`, `.text` RVA `0x1000`, raw pointer `0x400`, raw size `0x600`, virtual size `0x800`:

- RVA `0x1120` -> delta `0x120` -> raw offset `0x520` -> preferred VA `0x140001120`.
- RVA `0x1700` -> delta `0x700`, lies in virtual tail, no on-disk byte in that section.
- Loaded base `0x7FF600000000` -> actual VA for RVA `0x1120` is `0x7FF600001120` in that hypothetical session.
- If a report already says `0x1120` is an RVA, subtracting `0x140000000` again is invalid.

These are synthetic values; do not reuse them for any title. Verify mapping with two independent parsers or a known fixture and make the tool report the section and raw backing status.

### RIP-relative addressing

For x86-64, a memory operand relative to RIP uses the address after the instruction plus the sign-extended displacement. Synthetic instruction at `0x140001000`, length 7, disp32 `0x119`: referenced address is `0x140001120`. The referenced location may be a global variable slot, a pointer, a constant or a table. The instruction alone does not say what is stored there. Distinguish `lea` (address calculation) from a `mov` that loads the contents. A relocation or xref is evidence of linkage, not of runtime object identity.

## 3. Layout, alignment and native ABI

A compiler may insert padding before fields and at the end of an aggregate. Under ordinary MSVC x64 alignment, this illustrative POD:

```cpp
struct SampleRecord {
    uint8_t kind;
    uint32_t count;
    double reading;
};
```

usually has field offsets 0, 4, 8 and size 16; confirm with `offsetof` and `sizeof` in the exact build, not intuition. It has three padding bytes after kind. A packed file format with the same declared fields may use offsets 0, 1, 5 and length 13. A compiler-generated C++ object may have a vptr, base subobjects, virtual-base information and nontrivial lifetime. Standard-layout guarantees do not extend to arbitrary classes. A type's logical properties are not necessarily fields: some are computed, cached, stored indirectly or represented in a different object.

MSVC x64 uses four positional argument slots with integer and floating registers, stack home space and alignment rules [M04,M05]. Other ABIs differ. Inlining can eliminate a function body; identical-code folding can merge methods, and thunk addresses can differ from their underlying implementation. Generic sharing can create one implementation used by multiple declarations. An address alone is not a unique semantic name.

### Field inference evidence ladder

1. Source/debug metadata for an owned fixture provides intended type.
2. A matching binary has an access using a specific displacement, with a provable base object and operand width.
3. Multiple independent call sites or constructors support the same interpretation.
4. An authorized fixture with a known value supports the type, lifetime and units.
5. A distinct build comparison establishes whether the observation persisted or changed.

Do not convert an isolated decompiler name, a common displacement, or a string reference into a confirmed field. If only a use site is known, record width and instruction, leave semantics unresolved. A failed validation must downgrade the claim.

## 4. Containers, strings and object identity

- Arrays may be contiguous elements, arrays of references, variable-length records or segmented storage. Count is not necessarily capacity; index must be nonnegative and below count.
- Dynamic arrays may reallocate and invalidate references; hash maps can contain tombstones, bucket metadata and sparse indices. A dictionary's logical enumeration order is not a stable physical order.
- Strings may be UTF-8, UTF-16, interned identifiers, slices or length-prefixed buffers. Null-termination is not universal; use explicit bounds and encoding. Invalid Unicode should be reported rather than silently converted to a different key.
- Handles can encode an index and generation. An index reused after deletion must not be mistaken for an earlier object. Weak references and managed handles require runtime cooperation for safe lifetime.
- ECS architectures may store components in separate dense arrays or archetype chunks. An entity ID is not a pointer to a monolithic Player struct. Component migration can move a row without changing logical identity.

### Snapshot consistency example

Fixture generates tick 40 with object `(id=5,generation=1)` and position `(0,0,0)`. Tick 41 deletes it and creates `(5,2)` at `(10,0,0)`. A consumer that keys solely on `id` erroneously computes velocity across two distinct lifetimes. Correct processing resets history when generation changes. A multi-field sample from different ticks can create a non-existent state; use a producer-defined sequence and coherent copy where available.

## 5. Static analysis versus runtime and forensic dumps

A file shows original bytes, not necessarily loader-modified memory. A crash dump can omit pages. A live snapshot may mix times. A virtual machine snapshot may store compressed or sparse pages. An interpreter must map the correct source and address context before decoding. Volatility and MemProcFS are memory forensics tools [H06,R06]; choose a supported snapshot and OS symbol/version combination. A translation failure, absent page and valid zero are distinct. Never silently reinterpret missing bytes as a zero-initialized object.

## 6. Version manifest and typed evidence schema

Each observation should use a stable record with the following fields (example):

```json
{
  "record_id":"fixture-field-001",
  "artifact_sha256":"<actual hash required>",
  "build":"owned-fixture-v1",
  "arch":"x86-64",
  "tool":"IDA (record installed version)",
  "kind":"field_displacement",
  "value":"0x8",
  "owner":"SampleRecord",
  "width_bytes":8,
  "evidence":{"method":"source and disassembly","location":"fixture function + exact instruction"},
  "status":"pending",
  "limitations":["not independently executed"]
}
```

Allowed statuses: `reported`, `hypothesis`, `statically_verified`, `fixture_verified`, `unresolved`, `superseded`. Store preferred VA, RVA and raw offset as separate values where relevant; include the module identity and source hash. Schema validation should reject ambiguous `address` fields. A build change invalidates assumptions until rechecked. Retain old records as historical evidence rather than mutating the value and losing provenance.

## 7. Application to the existing notes (limited to audit)

The prior project notes mention metadata version 39, a reported successful wklin fork dump, registration-address labels, TypeInfo RVAs, and a mix of old and fresh build observations. Those claims are useful provenance, not universal knowledge. Registration values larger than the stated preferred image base are candidates for preferred-VA classification; confirm the tool's actual output convention before converting. TypeInfo slots, runtime class objects and instances are different concepts. An RVA from a prior build cannot be promoted to a current value because a neighboring field appears unchanged. This addendum does not reproduce or update competitive-game live offsets.

## 8. Validation matrix (planned, not executed)

| Input | Expected | Failure case |
|---|---|---|
| Owned PE with known sections | Correct raw/RVA/VA conversion | virtual tail or out-of-bounds rejected |
| POD compiled with symbols | Matching sizeof, field widths, offsets | wrong packing or architecture rejected |
| Synthetic record with invalid length | explicit schema error | no partial decode passed as complete |
| Two builds with changed layout | new manifest and diff | stale evidence blocked |
| Deleted and reused ID | reset history | no cross-generation association |
| Offline dump with missing page | missing-page error | no fabricated zero |

No tests or executable inspection were performed during authoring.
