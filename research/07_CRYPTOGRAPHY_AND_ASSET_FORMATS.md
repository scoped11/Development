# Cryptography, serialization, compression and asset formats

Companion to the unchanged RESEARCH_DECRYPTION.md, which already covers XOR, RC4, AES, ChaCha20, PQC and CNSA. This adds artifact classification, precise inputs and validation. Sources C01–C14,E09,M06. Work only with files and keys you own or are authorized to analyze.

## 1. Representation pipeline

Serialization, compression, encryption, authentication, signing and packaging are different. A file may have plaintext headers and compressed/encrypted entries. Authentication can cover framing and associated metadata. Determine actual ordering from the producer/consumer. High entropy is not proof of encryption; compressed data and random padding are alternatives.

| Operation | Required input | Output | Failure distinction |
|---|---|---|---|
| Serialize | schema, values, endian | byte representation | invalid type/length |
| Compress | codec, data, dictionary if needed | compressed frame | framing/dictionary/size error |
| Symmetric encryption | key, mode, nonce/IV | ciphertext | wrong input can yield plausible bytes |
| AEAD | key, nonce, plaintext, AAD | ciphertext/tag | reject authentication failure |
| Hash | bytes, algorithm | digest | compare to trusted expected digest |
| Signature | message, key | signature | verify with trusted public key |
| KDF | secret, salt/context, length | derived key material | different parameters yield different key |
| KEM | keypair/ciphertext | shared secret | not a bulk cipher |

## 2. Header classification and bounds

Read bounded prefix plus declared metadata; validate size before allocation. Record original hash, exact offsets, endian, declared lengths and actual size. Magic is only a clue. Archives require path safety, overlap/bounds and resource limits. Never execute extracted content as a parser step.

| Candidate | Common clue | Validation |
|---|---|---|
| PE | MZ and later PE | e_lfanew, COFF, sections, bounds [M06] |
| Zstandard | 28 B5 2F FD | frame/block structure, checksum if used [C07] |
| gzip | 1F 8B | method, flags, trailer, members [C10] |
| ZIP | PK signatures | directory/local headers, paths, sizes [C11] |
| JSON | text/whitespace | complete parse and schema |
| Protobuf | tags/varints, no fixed magic | schema, wire types, lengths [C09] |
| Unity AssetBundle | documented engine packaging | producer/version, entry structure [E09] |
| Custom | framing and entry table | count, overflow, offsets, overlap |

### Synthetic byte-level fixture

Invented format LAB1: 4-byte magic, u16 version, u16 flags, u32 payload length (little-endian), payload. Header is 12 bytes. Bit0 of flags = compressed; other bits unsupported.

```text
4C 41 42 31 01 00 00 00 04 00 00 00 41 42 43 44
```

Expected v1, flags=0, length=4, payload ASCII ABCD, total 16. Length 0xFFFFFFFF must be rejected before allocation. Flag 0x0002 unsupported. Compressed flag requires documented codec and output cap. Version 2 can add explicit header length; v1 parser rejects unsupported versions. This is not a game format. Protobuf `08 96 01` = field 1, wire 0, varint 150. `0A 03 61 62 63` = field 1, length-delimited bytes abc; semantics require schema [C09]. Reject unterminated varints and lengths exceeding remaining bytes.

## 3. Parameters, authentication and cryptographic identification

AES has a defined block primitive, mode is separate [C01–C03]. CBC requires IV/padding rules, CTR counter layout/position, GCM nonce, AAD and tag. A 16-byte block is insufficient to identify mode. ChaCha20's IETF format and Poly1305 authentication are specified separately [C04]. Recognizable arithmetic alone does not establish the full cipher. XOR reversibility is not key identification. RC4 is legacy; vectors verify implementation behavior [C05]. Salt, nonce, key, key identifier and public verification key are distinct. HKDF takes input key material, salt/context and requested output length [C08]; it is not by itself password hardening. A key ID does not contain the secret.

### AEAD fixture input and negative cases

Use maintained implementation with published vector: explicit algorithm, key, nonce, plaintext, associated data, expected ciphertext/tag. Compare expected bytes, then change one bit of ciphertext, tag or AAD; wrong key and truncation must also fail. Do not release unauthenticated plaintext as a success. A round-trip can pass even when implementation differs from a standard. RFC 5116 specifies AEAD interface [C06]. No vector executed here.

## 4. PQC and signature semantics

FIPS 203 ML-KEM establishes shared secret; FIPS 204 ML-DSA and FIPS 205 SLH-DSA specify signature schemes [C12–C14]. FIPS 203/204 pages contain planning/errata notes; track edition. A software-update signature verifies authenticity relative to a trusted key and does not imply payload confidentiality. Do not infer a particular government's algorithm from a string or unexplained blob. Prior guide covers CNSA separately; no new claim of a target cipher is made.

## 5. Asset parsing and evidence package

Create a small owned project with known assets and build settings. Compare producer manifest, extracted entry count, decoded size and source semantics. A container can include multiple compression methods and framing types. Parse in a bounded sandbox; preserve source and intermediate hashes, exact parameters, tool release, output path, warnings and offset of errors. No secret or missing schema should be fabricated. If required material is not available within authorization, record unresolved.

```json
{"artifact":"owned-lab.bin","sha256":"<actual hash required>","size_bytes":16,"format_hypothesis":"LAB1","header_offset":0,"payload_offset":12,"payload_length":4,"transform_chain":["parse"],"status":"synthetic_expected"}
```

Keep format recognition, cipher identification, parsing failures and transport failures separate. No protected target decryption or new binary evidence was produced.
