# ValveResourceFormat (VRF) & Source 2 Compiled Resource Reference

Technical specification of Source 2 asset compilation, VPK container indexing, and resource decompilation based on [ValveResourceFormat](https://github.com/ValveResourceFormat/ValveResourceFormat).

---

## 1. VPK (Valve Package) Container Format

Source 2 packages game assets into multi-part VPK archives (`pak01_dir.vpk` + `pak01_000.vpk`, `pak01_001.vpk`, etc.):

### Directory Header (`pak01_dir.vpk`)
```
+-------------------------------------------------------------+
| Header:                                                     |
|   uint32 Signature      = 0x55AA1234                        |
|   uint32 Version        = 2                                 |
|   uint32 TreeSize       = Size of file index tree           |
|   uint32 FileDataSectionSize = 0 (for multi-part packages)  |
|   uint32 ArchiveMD5SectionSize                              |
|   uint32 OtherMD5SectionSize                                |
|   uint32 SignatureSectionSize                               |
+-------------------------------------------------------------+
```

### Tree Index Hierarchy
The file tree is arranged as:
`Extension` -> `Path` -> `Filename` -> `Entry Metadata`:
- `CRC`: uint32 checksum of file payload
- `PreloadBytes`: Length of data stored directly inside the directory file (often 0)
- `ArchiveIndex`: uint16 index of target `pak01_XXX.vpk` archive
- `EntryOffset`: uint32 byte offset within archive
- `EntryLength`: uint32 byte size of file payload
- Terminator `0xFFFF`

---

## 2. Compiled Resource (`_c`) Binary Layout

Every file packaged inside Source 2 VPK archives ends with `_c`.

```
+-------------------------------------------------------------+
| Resource Header (16 bytes):                                 |
|   uint32 FileSize       Total size including header         |
|   uint16 HeaderVersion  0x000C                              |
|   uint16 Version        0x0001                              |
|   uint32 BlockOffset    Offset to block directory (0x10)    |
|   uint32 BlockCount     Number of binary blocks             |
+-------------------------------------------------------------+
| Block Directory Entries:                                    |
|   char[4] BlockTag      "DATA", "REDI", "NTRO", "VBIB"      |
|   uint32  Offset        Relative offset from header end     |
|   uint32  Size          Byte size of block payload          |
+-------------------------------------------------------------+
| Block Payloads:                                             |
|   [DATA Block] -> Serialized asset data (KV3 / bytecode)    |
|   [REDI Block] -> Compiler dependencies & argument metadata |
|   [NTRO Block] -> Schema introspection for unknown structs  |
+-------------------------------------------------------------+
```

### Panorama Compiled Types
- **`vxml_c`**: Binary KeyValues3 document encoding DOM element hierarchy, panel classes, and embedded script/style references.
- **`vcss_c`**: Binary KeyValues3 table of CSS selectors, property tokens, color constants, and keyframe animation tracks.
- **`vjs_c`**: Compiled JavaScript bytecode or UTF-8 string tables containing Panorama controller logic.

---

## 3. KeyValues3 (KV3) Serialization Format

KeyValues3 is Valve's polymorphic object model. In binary form, it begins with an encoding header:

### Format Header GUIDs
- **Binary KV3 Encoding (Uncompressed)**: `46 56 4B 33` (`VK3\x01`)
- **Binary KV3 (BlockLZ4)**: `46 56 4B 33` followed by LZ4 frame chunking
- **Text Format Identifier**: `<!-- e227f01f-0325-4c4e-81f4-56c607a9a1ac -->`

### KV3 Data Types
| ID | Type | Description |
| :--- | :--- | :--- |
| `0x01` | `null` | Null identifier |
| `0x02` | `bool` | 1-byte boolean flag |
| `0x03` | `int64` | Signed 64-bit integer |
| `0x04` | `uint64` | Unsigned 64-bit integer |
| `0x05` | `double` | 64-bit IEEE floating point |
| `0x06` | `string` | Length-prefixed or null-terminated UTF-8 |
| `0x07` | `binary_blob`| Raw byte buffer |
| `0x08` | `array` | Sequential array of KV3 values |
| `0x09` | `object` | Key-value associative mapping table |

---

## 4. Decompiling Assets Using ValveResourceFormat (VRF)

```bash
# Decompile entire pak01_dir.vpk panorama assets to readable XML/CSS/JS:
Decompiler.exe -i "game/csgo/pak01_dir.vpk" -e "panorama" -o "./decompiled_panorama/"

# Decompile single compiled resource:
Decompiler.exe -i "demo_viewer.vxml_c" -o "./demo_viewer.xml"
```
