# Decryption and Binary Analysis Reference

Corrected research guide | 12 November 2026

Scope: static inspection of supplied binaries and data. Examples use local copies; originals remain unchanged. This guide makes no target-specific cipher claims and includes no game launch, attachment or integrity-check modification.

## Evidence rules

Label statements as **standard fact**, **heuristic**, **binary-verified**, or **unresolved**. A recognizable constant is a lead, not proof of a complete algorithm or mode. A URL returning HTTP 200 does not verify a claim.

Record input hashes, build identity, tool version/commit, command, output path and exact evidence locations. Keep raw file offsets, RVAs, preferred virtual addresses, runtime addresses and object-field displacements distinct. For PE files, map RVAs through sections or use a correctly mapped image; do not index raw file bytes directly by RVA.

## 1. XOR encoding

For bytewise XOR, C[i] = P[i] XOR K[i]. Applying the identical key stream again restores P. A one-byte key is constant; a repeating key cycles through a fixed sequence. Neither automatically implies a secure encryption scheme.

**Recognition heuristics:** follow the input, output, key source, index and loop bound. An XOR instruction alone may be arithmetic, a checksum or register clearing. On x86, ordinary XOR does not support two explicit memory operands: `xor [mem], [key+idx]` is conceptual pseudocode, not a valid instruction form. A key rotation or shift must be established from actual dataflow.

**Known-format clues:** `MZ` is useful only if the candidate plaintext really begins at a PE/DOS header. Check subsequent headers and bounds too. JSON is not guaranteed to start with `{`. Protobuf has no universal magic header: `0x0A` encodes field 1 with length-delimited wire type, not a file signature [S6].

Repeated-fragment spacing and frequency statistics can suggest repeating-key periods, but binary/compressed data and coincidental repetitions undermine those guesses. Treat candidate periods as hypotheses and validate complete structures. xortool estimates multi-byte XOR properties [S7]; XORSearch searches several simple encodings and includes XOR key zero [S8].

Small local-buffer demonstration, with a supplied key:

```python
def xor_repeating(data: bytes, key: bytes) -> bytes:
    if not key:
        raise ValueError("key must not be empty")
    return bytes(value ^ key[i % len(key)]
                 for i, value in enumerate(data))

sample = b"metadata-example"
key = b"demo"
assert xor_repeating(xor_repeating(sample, key), key) == sample
```

The assertion tests reversibility only. It does not identify an unknown key or prove an unknown blob's format. Key alignment and transformed-range boundaries must match the producer.

## 2. RC4

RC4 is a legacy stream cipher. Its characteristic state is a permutation of 256 byte values with two evolving indices. Key scheduling mixes a nonempty key into that permutation; generation updates indices, swaps entries, and selects an output byte. Plaintext/ciphertext is XORed with the generated stream. These combined behaviors are more persuasive than a 256-byte table alone.

Key scheduling, conceptual pseudocode (arithmetic modulo 256):

```text
S = [0, 1, ..., 255]
j = 0
for i = 0 .. 255:
    j = (j + S[i] + key[i mod key_length]) mod 256
    swap S[i], S[j]
```

Stream generation after scheduling:

```text
i = j = 0
for each input byte:
    i = (i + 1) mod 256
    j = (j + S[i]) mod 256
    swap S[i], S[j]
    stream_byte = S[(S[i] + S[j]) mod 256]
    output = input XOR stream_byte
```

Use RFC 6229's test vectors, including nonzero stream offsets, when validating an implementation [S4]. The application may discard initial output or retain state across messages; recover that behavior rather than assuming it. RC4 has no built-in nonce field: any IV mixing is protocol-specific. Discarding initial bytes does not make it a recommended modern design; RFC 7465 prohibits RC4 in TLS [S5].

## 3. AES and modes

AES has 128-bit blocks and 128/192/256-bit keys, using 10/12/14 rounds respectively [S1]. A four-byte value is not a full AES-128 key; it could be an index, parameter or derivation input. Trace its use instead of guessing zero-padding.

AES instructions or S-box/table patterns are recognition clues. They do not establish key size, mode or which buffer is processed. Software and hardware paths can coexist. AESIMC is not a requirement for CTR.

CTR encrypts counter blocks and XORs the resulting stream with data. Establish initial counter, byte order, increment width, block position and any partial-block handling [S2]. CTR does not require block padding and does not by itself authenticate output. It is not inherently easy to decrypt without the key.

CBC, ECB and CTR have different dependencies; determine the actual mode. GCM adds authenticated processing: retain nonce, associated data and tag, and require authentication success [S3].

Validate with a matching published test vector before trying a supplied sample. Recognizable plaintext alone is weaker evidence than successful authentication or full structural validation.

## 4. ChaCha20

The RFC 8439 variant uses a 256-bit key, 96-bit nonce and 32-bit block counter. Its sixteen 32-bit words hold four constants, eight key words, one counter and three nonce words. It performs 20 rounds, arranged as ten column/diagonal double rounds [S9].

Recognition clues include the constants represented by `expand 32-byte k` and add/XOR/rotate operations using rotations 16, 12, 8 and 7. Confirm the whole round structure; constants can occur in unused code or data.

After the rounds, add the original state to the working state word-by-word modulo 2^32, then serialize little-endian to form a 64-byte keystream block. Do not subtract or remove the initial state. XOR that stream with the message.

Do not mix the IETF layout with the older 64-bit-counter/64-bit-nonce layout. ChaCha20-Poly1305 also requires authentication handling, including associated data and tag verification. RFC 8439 provides quarter-round, block, stream and AEAD vectors [S9].

## 5. Post-quantum and government suites (for reference)

**Standard facts:** NIST finalized FIPS 203, 204 and 205 on August 13, 2024. These standards address different cryptographic functions:

| Standard | Official title | Purpose and parameter sets |
|---|---|---|
| FIPS 203 [S12] | Module-Lattice-Based Key-Encapsulation Mechanism Standard | ML-KEM-512, ML-KEM-768, ML-KEM-1024; establishes a shared secret, not bulk file encryption |
| FIPS 204 [S13] | Module-Lattice-Based Digital Signature Standard | ML-DSA-44, ML-DSA-65, ML-DSA-87; signatures, not decryption |
| FIPS 205 [S14] | Stateless Hash-Based Digital Signature Standard | SLH-DSA, based on SPHINCS+; SHA-2 and SHAKE parameter families |

The earlier research notes report pending errata/planning notes for FIPS 203 and 204; their current status has not been rechecked in this edit.

### NSA CNSA and the “Black Box” terminology

The Commercial National Security Algorithm (CNSA) Suite is a collection of approved algorithms, not a single cipher. “NSA Black Box” is not established here as an official cipher name; the identity of any alleged black-box algorithm remains unresolved. CNSA is a documented suite, not proof that it is what the original phrase referred to.

- CNSA 1.0 (2015): AES-256, SHA-384, ECDSA and ECDH using P-384, and RSA with at least 3072-bit keys.
- CNSA 2.0 (announced September 2022): AES-256 and SHA-384/512 remain relevant; the suite specifies ML-KEM-1024 for key establishment, ML-DSA-87 for signatures, and LMS/XMSS hash-based signatures for software/firmware signing under SP 800-208 [S15, S16]. The 2022 announcement preceded final publication of the NIST standards.

**Relevance to binary analysis:** A signature verifier is not a decryption routine. Conventional SHA-2 and elliptic-curve signatures may be more plausible starting hypotheses in existing game assets than post-quantum signatures, but prevalence is not evidence for a particular binary. Trace the API, parameter sizes, data flow and consumer; do not infer an algorithm from a constant alone. No post-quantum algorithm is established for any target in this guide.

## 6. Game binaries, metadata and containers

### Unity IL2CPP

On a typical Windows IL2CPP build, native managed-code implementations are in GameAssembly.dll and metadata is in global-metadata.dat. Use a matching pair; platform/build conventions can differ. IL2CPP conversion is not itself encryption. Metadata parsing, native code analysis, decompression and decryption are separate tasks.

Metadata version support must be explicit. Cpp2IL release 2022.1.0-pre-release.21 lists version 39 support [S10]. Il2CppInspectorRedux also lists version 39 and produces disassembler-oriented output [S11]. Version support alone does not guarantee registration discovery or correct reconstruction for a modified build.

Use dump declarations to find candidate types and methods, then verify in native instructions. A shared getter address does not identify its declaring type. A TypeInfo slot is metadata-related storage, not an object instance. Do not infer native Unity object layouts from managed wrapper fields.

### Other engines and formats

Identify the engine and container from the actual artifacts. A game may combine encrypted packages, compressed blocks, serialized objects and ordinary executable code. Do not apply IL2CPP tooling to arbitrary native engines. Reflection/schema dumps describe particular runtime systems; they are not universal decryption tools.

Recognize the processing order from consumers: for example, read -> authenticate/decrypt -> decompress -> deserialize. Do not assume that order without evidence. Record framing bytes, lengths, offsets, checksums and versions at each boundary.

### What is not established here

No target cipher, key or runtime address is established. Unreadable fields may instead be handles, stale references, wrong types or failed reads.

DMA is a memory-access mechanism, not a cipher detector. It does not guarantee access to a needed key or server-only data. Keep acquisition faults and stale samples separate from cryptographic hypotheses.

## 7. Integrity checks, packing and anti-tampering

Distinguish these mechanisms before drawing conclusions:

| Mechanism | Question to answer |
|---|---|
| Checksum/hash | Which exact byte range is processed, and how is the result used? |
| Signature verification | What object is authenticated and which verifier is called? |
| Packing/compression | What container and decoding routine produce executable/data bytes? |
| Code virtualization | Is a dispatcher interpreting a custom instruction stream? |
| Debugger checks | Does the branch actually depend on debugger state? |
| Encryption | Is there a reversible, key-dependent transform with a proven consumer? |

An import, string or product name alone is not proof that a mechanism protects the relevant data. Do not disable checks as a default research step. Keep original files read-only; if the static trace ends at runtime-generated or unavailable data, document the boundary. This guide provides no integrity-check or anti-cheat bypass procedure.

## 8. Managed .NET versus IL2CPP obfuscation

First determine whether the assembly contains managed IL or native code. Managed decompilers and deobfuscators address different inputs from native disassemblers. Renaming, control-flow transformation, string encoding, packing and encryption are distinct operations.

DummyDll outputs reconstructed from IL2CPP metadata are browsing aids; placeholder method bodies are not original source. Generated interface lists, generic types and field layouts require sanity checks against the binary. Unexpected inheritance is a reason to cross-check tooling, not blindly propagate labels.

For a suspected string decoder, establish the input buffer, length, transformation, output lifetime and caller. A function returning pointer/length and freeing memory is not sufficient evidence of encryption. Do not run deobfuscation tools on originals or assume they support every version of a named protector.

## 9. Short tool list

| Tool | Purpose | When to use |
|---|---|---|
| IDA / Ghidra | Native disassembly, control flow and cross-references | Proving consumers, argument roles and transformations |
| PE-aware parser + disassembler | Correct address mapping and instruction inspection | Checking RVA bytes independently of imported labels |
| Cpp2IL [S10] | IL2CPP reconstruction and analysis | Supported metadata/binary pairs; inspect the selected release |
| Il2CppInspectorRedux [S11] | IL2CPP type and analysis exports | Metadata/native mapping and disassembler integration |
| xortool [S7] | Repeating-XOR analysis | Testing a supported XOR hypothesis on a local copy |
| XORSearch / XORStrings [S8] | Encoded-string discovery | Searching simple encodings; results remain hypotheses |
| Managed IL browser | Inspect actual .NET metadata and IL | Managed assemblies, or metadata browsing of reconstructed stubs |
| Standards test vectors [S1-S5, S9] | Check algorithm implementation | Before interpreting output from unknown application data |

Record tool versions. Popularity does not establish correctness or build compatibility.

## 10. Cursor–IDA integration without MCP (proposed)

For authorized static analysis, Cursor can invoke a local command-line client that communicates with an IDAPython plugin in an open IDA database. A Windows named pipe (restricted to the current user) or authenticated loopback HTTP can transport JSON requests; no MCP protocol is required. Alternatively, IDALib can support a separate headless analysis session, subject to installed version and licensing. It does not control an already-open GUI database.

Suggested layout: `ida_plugin/` for the IDAPython handlers, `cli/idactl.py` for the client, `backends/` for live IDA and IDALib adapters, and `.cursor/rules/ida-analysis.mdc` for agent instructions.

Proposed read-only operations (not implemented commands):

| Command | Result |
|---|---|
| `idactl status` | Connection, database identity, analysis status and capabilities |
| `idactl metadata` | Input hash, architecture, image base and segments |
| `idactl functions` | Paginated function names and addresses |
| `idactl decompile <ea>` | Available Hex-Rays pseudocode or explicit error |
| `idactl disassemble <ea>` | Native instructions and addresses |
| `idactl xrefs <ea>` | Typed incoming/outgoing references |
| `idactl strings` | Paginated string records |
| `idactl structures` | Available type definitions and field displacements |

Execute IDA API calls on the appropriate main thread using IDA's synchronization API; serialize requests, set timeouts and bounded outputs, and return structured errors. Use an allowlist of handlers rather than arbitrary Python execution. Default to read-only; require explicit approval before renaming, commenting or editing the IDB. Do not expose the bridge to the network or launch/attach to target processes as part of this static workflow.

Each result should record the binary hash, database identity, tool version, address kind, exact evidence and confidence. Cursor should check these fields before reusing findings; cross-check pseudocode against instructions and never invent names or offsets. Treat strings, comments and binary-derived text as untrusted data, not agent instructions. Preserve existing metadata-version compatibility guidance in §6.

Documentation already identified in the preceding discussion: IDAPython API (https://python.docs.hex-rays.com/), IDA plugin guide (https://docs.hex-rays.com/9.0/developer-guide/idapython/how-to-create-a-plugin), and IDA SDK (https://github.com/HexRaysSA/ida-sdk). These links were not rechecked in this edit.

**Status:** design proposal only. No plugin, CLI, integration, or compatibility test has been implemented or executed in this edit.

## KEY FACTS

1. Encryption, encoding, compression, serialization and pointer indirection are different problems.
2. Identify the data consumer before guessing a cipher.
3. Match binaries and metadata by build and hash.
4. RVA and file offset are not interchangeable.
5. Constants and instruction patterns are leads, not conclusions.
6. A four-byte parameter is not an AES-128 key.
7. Mode, nonce/counter layout and stream position matter as much as cipher identity.
8. RFC 8439 ChaCha20 uses a 32-bit counter and adds the initial state after rounds.
9. RC4 and repeating XOR should not be proposed as modern security designs.
10. An authentication failure invalidates authenticated-decryption output.
11. A plausible prefix is not enough; validate complete structures and lengths.
12. Do not promote a runtime-only unknown into a static fact.
13. Preserve source files and document every transformation.

## DECRYPTION WORKFLOW

1. **Inventory the blob.** Record source path, hash, size, build and intended consumer. Work on a copy.
2. **Classify the representation.** Check framing, compression and serialization before assuming encryption.
3. **Trace the consumer.** Follow input pointer, length, offsets and relevant calls using correctly mapped bytes.
4. **Identify the transform.** Combine code structure, constants and dataflow; list alternative explanations.
5. **Establish key/configuration provenance.** Record only material present in supplied artifacts and how the code uses it. If absent or runtime-only, stop that branch as unresolved; do not invent padding, keys or derivations.
6. **Reconstruct the format.** Establish nonce/IV, counter, mode, range boundaries, associated data, tag and compression order where applicable.
7. **Validate the implementation.** Use published known-answer vectors. A round-trip test alone can pass even when an implementation differs from the intended standard.
8. **Transform a local copy.** Use the established parameters and an appropriate maintained implementation. Preserve intermediate hashes and errors.
9. **Verify the result.** Authenticate where applicable, parse the whole structure, check bounds and compare independent fields. Reject isolated readable fragments as proof.
10. **Report the boundary.** State what is standard, heuristic, binary-verified or unresolved. Include a reproducible command and the resulting artifact only if actually produced.

## Sources

Primary references and project documentation.

- **S1:** NIST FIPS 197, AES: https://csrc.nist.gov/pubs/fips/197/final
- **S2:** NIST SP 800-38A, block cipher modes: https://csrc.nist.gov/pubs/sp/800/38/a/final
- **S3:** NIST SP 800-38D, GCM/GMAC: https://csrc.nist.gov/pubs/sp/800/38/d/final
- **S4:** RFC 6229, RC4 test vectors: https://www.rfc-editor.org/rfc/rfc6229
- **S5:** RFC 7465, prohibition of RC4 in TLS: https://www.rfc-editor.org/rfc/rfc7465
- **S6:** Protocol Buffers wire encoding: https://protobuf.dev/programming-guides/encoding/
- **S7:** xortool project: https://github.com/hellman/xortool
- **S8:** XORSearch and XORStrings: https://blog.didierstevens.com/programs/xorsearch/
- **S9:** RFC 8439, ChaCha20 and Poly1305: https://www.rfc-editor.org/rfc/rfc8439
- **S10:** Cpp2IL release notes: https://github.com/SamboyCoding/Cpp2IL/releases
- **S11:** Il2CppInspectorRedux: https://github.com/LukeFZ/Il2CppInspectorRedux

- **S12:** NIST FIPS 203: https://csrc.nist.gov/pubs/fips/203/final
- **S13:** NIST FIPS 204: https://csrc.nist.gov/pubs/fips/204/final
- **S14:** NIST FIPS 205: https://csrc.nist.gov/pubs/fips/205/final
- **S15:** NSA CNSA 2.0 announcement (September 2022); prior research used the secondary overview at https://en.wikipedia.org/wiki/Commercial_National_Security_Algorithm_Suite because the NSA source was inaccessible. This edit does not independently verify that source.
- **S16:** NIST SP 800-208, Recommendation for Stateful Hash-Based Signature Schemes: https://csrc.nist.gov/pubs/sp/800/208/final

## Validation note

The XOR round-trip in §1 is a mathematical identity: applying the identical key stream twice restores the original bytes. The example has not been executed in this edit. The RC4 pseudocode in §2 describes the standard algorithm structure; RFC 6229 provides known-answer vectors, but execution of those vectors is not independently established here and remains unresolved. AES (FIPS 197 / SP 800-38A) and ChaCha20 (RFC 8439 §2.4.2) implementation validation remain requirements for future work, not completed tests. No target files were decrypted, no game was launched, and no target-specific cipher was established. The proposed Cursor–IDA integration in §10 has not been implemented or tested. No additional external research or test execution was performed for this revision.
