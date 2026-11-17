// =============================================================================
// Counter-Strike 2 (Source 2) - KeyValues3 (KV3) & Resource Container Simulator
// Target Subsystems: resourcesystem.dll (Base: 0x180000000)
//
// Capabilities Verified:
//   1. Text KeyValues3 Syntax Tokenizer, Header Parser & Dynamic AST Traversal
//   2. Binary KeyValues3 (VKV3) Stream Encoding, String Dictionary & Deserialization
//   3. Block Compression Engine: Pure ISO C++20 LZ4 Block Codec
//   4. ZSTD Frame Specification Inspector & Block Decoding Representation
//   5. Source 2 Compiled Resource Container (ResourceHeader_t / "VASU") Unpacking
//   6. Schema-Driven Recursive Object Mutation, Dynamic Field Extraction & Type Safety
//
// Compliance: Pure ISO C++20, MSVC x64 (/O2 /std:c++20 /EHsc /W4 /WX), zero external dependencies.
// =============================================================================

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <string_view>
#include <cstdint>
#include <cassert>
#include <span>
#include <sstream>

// Include authoritative reconstructed KeyValues3 & Resource System engine implementation
#include "../../game/resourcesystem/kv3_parser.h"
#include "../../game/resourcesystem/kv3_parser.cpp"

using namespace cs2::resourcesystem;

namespace {

void PrintSeparator(char ch = '=') {
    for (int i = 0; i < 80; ++i) std::cout << ch;
    std::cout << '\n';
}

void PrintTestHeader(int testNum, const char* title) {
    std::cout << "\n[TEST " << testNum << "] " << title << "\n";
}

} // anonymous namespace

int main() {
    PrintSeparator('=');
    std::cout << " Source 2 KeyValues3 (KV3) Serialization & Asset Container Suite\n";
    std::cout << " Target: resourcesystem.dll | Container: \"VASU\" (Magic: 0x55534156)\n";
    std::cout << " Standard: Pure ISO C++20 | MSVC x64 (/std:c++20 /O2 /EHsc /W4 /WX)\n";
    PrintSeparator('=');

    int testsPassed = 0;
    int totalTests = 0;

    // =========================================================================
    // TEST 1: Text KV3 Syntax Parsing & Dynamic AST Traversal
    // =========================================================================
    totalTests++;
    PrintTestHeader(1, "Text KV3 Syntax Parsing & Dynamic AST Traversal");
    {
        const std::string textKv3Source =
            "<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} "
            "format:generic:version{74477221-4c60-44d8-a14a-03838b558f68} -->\n"
            "{\n"
            "    m_sResourceName = \"models/weapons/v_rifle_primary.vmdl\"\n"
            "    m_nPrimaryID = 42\n"
            "    m_flMassKg = 3.450000\n"
            "    m_bIsSerialized = true\n"
            "    m_pNullReference = null\n"
            "    m_aBoundingExtents = [ -16.000000, -8.500000, 0.000000, 16.000000, 8.500000, 32.000000 ]\n"
            "    m_subHierarchy = \n"
            "    {\n"
            "        m_sBoneAttachment = \"weapon_hand_R\"\n"
            "        m_nBoneIndex = 7\n"
            "    }\n"
            "    m_rawSignature = #[ 56 41 53 55 01 00 0C 00 ]\n"
            "}\n";

        CKV3Document doc;
        bool bParsed = doc.LoadFromText(textKv3Source);

        std::cout << "   Parsing Text KV3 Document... " << (bParsed ? "SUCCESS" : "FAIL") << "\n";
        assert(bParsed);

        // Verify Header Metadata
        std::cout << "   Encoding GUID: " << doc.m_header.m_encodingGuid.ToString() << "\n";
        std::cout << "   Format GUID:   " << doc.m_header.m_formatGuid.ToString() << "\n";
        assert(doc.m_header.m_encodingGuid == KV3Guid_t(g_GuidTextEncoding));
        assert(doc.m_header.m_formatGuid == KV3Guid_t(g_GuidGenericFormat));

        // Field extraction
        const auto& root = doc.Root();
        assert(root.IsTable());
        assert(root.HasKey("m_sResourceName"));
        assert(root["m_sResourceName"].AsString() == "models/weapons/v_rifle_primary.vmdl");
        assert(root["m_nPrimaryID"].AsInt() == 42);
        assert(std::abs(root["m_flMassKg"].AsDouble() - 3.45) < 1e-4);
        assert(root["m_bIsSerialized"].AsBool() == true);
        assert(root["m_pNullReference"].IsNull());

        // Array traversal
        assert(root["m_aBoundingExtents"].IsArray());
        assert(root["m_aBoundingExtents"].Size() == 6);
        assert(root["m_aBoundingExtents"][0].AsDouble() == -16.0);
        assert(root["m_aBoundingExtents"][5].AsDouble() == 32.0);

        // Subtable traversal
        assert(root["m_subHierarchy"].IsTable());
        assert(root["m_subHierarchy"]["m_sBoneAttachment"].AsString() == "weapon_hand_R");
        assert(root["m_subHierarchy"]["m_nBoneIndex"].AsInt() == 7);

        // Binary Blob extraction
        assert(root["m_rawSignature"].IsBlob());
        const auto& blob = root["m_rawSignature"].AsBlob();
        assert(blob.size() == 8);
        assert(blob[0] == 0x56 && blob[1] == 0x41 && blob[2] == 0x53 && blob[3] == 0x55);

        // Round-trip formatting
        std::string generatedText = doc.SaveToText();
        assert(!generatedText.empty());
        assert(generatedText.find("models/weapons/v_rifle_primary.vmdl") != std::string::npos);

        std::cout << "   [PASS] Text KV3 AST parsing and field verification verified!\n";
        testsPassed++;
    }

    // =========================================================================
    // TEST 2: Binary KeyValues3 (VKV3) Serialization & Deserialization
    // =========================================================================
    totalTests++;
    PrintTestHeader(2, "Binary KeyValues3 (VKV3) Serialization & Deserialization");
    {
        CKV3Document srcDoc;
        srcDoc.m_root = KV3Table{};
        srcDoc.m_root["m_sEngineSystem"] = "Source2ResourceCompiler";
        srcDoc.m_root["m_nBuildRevision"] = 10420;
        srcDoc.m_root["m_flTickInterval"] = 0.015625;
        srcDoc.m_root["m_bSubtickEnabled"] = true;

        KV3Array animSequences;
        animSequences.push_back("idle_active");
        animSequences.push_back("deploy_draw");
        animSequences.push_back("fire_primary");
        srcDoc.m_root["m_aSequences"] = std::move(animSequences);

        KV3Table physicsProps;
        physicsProps.emplace_back("m_flElasticity", 0.35);
        physicsProps.emplace_back("m_flFriction", 0.75);
        physicsProps.emplace_back("m_nMaterialIndex", 12);
        srcDoc.m_root["m_physicsProps"] = std::move(physicsProps);

        // Serialize to Uncompressed VKV3 Stream
        std::vector<std::uint8_t> binaryStream = srcDoc.SaveToBinary(KV3CompressionType::None);
        std::cout << "   Generated VKV3 Stream: " << binaryStream.size() << " bytes\n";
        assert(binaryStream.size() >= 24);

        // Check VKV3 Magic
        assert(binaryStream[0] == 'V' && binaryStream[1] == 'K' && binaryStream[2] == 'V' && binaryStream[3] == '3');

        // Deserialize back from binary
        CKV3Document dstDoc;
        bool bLoaded = dstDoc.LoadFromBinary(binaryStream);
        assert(bLoaded);

        // Verify data fidelity
        assert(dstDoc.m_root["m_sEngineSystem"].AsString() == "Source2ResourceCompiler");
        assert(dstDoc.m_root["m_nBuildRevision"].AsInt() == 10420);
        assert(std::abs(dstDoc.m_root["m_flTickInterval"].AsDouble() - 0.015625) < 1e-6);
        assert(dstDoc.m_root["m_bSubtickEnabled"].AsBool() == true);
        assert(dstDoc.m_root["m_aSequences"].Size() == 3);
        assert(dstDoc.m_root["m_aSequences"][1].AsString() == "deploy_draw");
        assert(dstDoc.m_root["m_physicsProps"]["m_nMaterialIndex"].AsInt() == 12);

        // Value tree equality check
        assert(srcDoc.Root() == dstDoc.Root());

        std::cout << "   [PASS] Binary KeyValues3 round-trip lossless encoding verified!\n";
        testsPassed++;
    }

    // =========================================================================
    // TEST 3: Pure ISO C++20 LZ4 Block Compression Engine
    // =========================================================================
    totalTests++;
    PrintTestHeader(3, "Pure ISO C++20 LZ4 Block Compression & Decompression");
    {
        // Construct highly redundant data buffer
        std::vector<std::uint8_t> rawData;
        const char pattern[] = "SOURCE2_RESOURCESYSTEM_BLOCK_DECOMPRESSION_TEST_PAYLOAD_STRING_PATTERN_1234567890";
        for (int i = 0; i < 40; ++i) {
            for (char c : pattern) {
                rawData.push_back(static_cast<std::uint8_t>(c));
            }
        }

        std::cout << "   Uncompressed Test Buffer: " << rawData.size() << " bytes\n";

        // Compress
        std::vector<std::uint8_t> compressed = CLZ4Codec::Compress(rawData);
        std::cout << "   LZ4 Compressed Buffer:    " << compressed.size() << " bytes\n";
        double ratio = static_cast<double>(compressed.size()) / static_cast<double>(rawData.size()) * 100.0;
        std::cout << "   Compression Ratio:        " << std::fixed << std::setprecision(2) << ratio << "%\n";
        assert(compressed.size() < rawData.size());

        // Decompress
        std::vector<std::uint8_t> decompressed(rawData.size(), 0);
        bool bDecomp = CLZ4Codec::Decompress(compressed, decompressed);
        assert(bDecomp);
        assert(decompressed == rawData);

        // Test LZ4 Binary KV3 container
        CKV3Document doc;
        doc.m_root = KV3Table{};
        for (int i = 0; i < 20; ++i) {
            std::string key = "prop_entry_" + std::to_string(i);
            doc.m_root[key] = "repetitive_value_string_for_compression_testing_purposes";
        }

        std::vector<std::uint8_t> lz4Kv3 = doc.SaveToBinary(KV3CompressionType::LZ4);
        std::cout << "   LZ4-Compressed VKV3 File: " << lz4Kv3.size() << " bytes\n";

        CKV3Document restoredDoc;
        bool bRestored = restoredDoc.LoadFromBinary(lz4Kv3);
        assert(bRestored);
        assert(restoredDoc.Root() == doc.Root());

        std::cout << "   [PASS] Standalone LZ4 block compression/decompression verified!\n";
        testsPassed++;
    }

    // =========================================================================
    // TEST 4: ZSTD Frame Specification & Block Decoding Representation
    // =========================================================================
    totalTests++;
    PrintTestHeader(4, "ZSTD Frame Specification & Block Decoding Representation");
    {
        // Build synthetic ZSTD raw frame
        // Magic: 0xFD2FB528
        // FHD: Single segment (bit 5 = 1), FCS flag = 1 (2 bytes size)
        // Block Header: 3 bytes (Last=1, Type=Raw (0), Size=N)
        std::vector<std::uint8_t> zstdFrame;

        // Magic
        zstdFrame.push_back(0x28);
        zstdFrame.push_back(0xB5);
        zstdFrame.push_back(0x2F);
        zstdFrame.push_back(0xFD);

        // FHD: Single segment (0x20) | FCS 1 byte (0x00) = 0x20
        zstdFrame.push_back(0x20);

        const std::string rawPayload = "ValveSource2KeyValues3ZstdFramePayload";
        auto payloadSize = static_cast<std::uint32_t>(rawPayload.size());

        // Window descriptor is omitted when Single Segment is set
        // Frame Content Size (1 byte when Single_Segment is 1 and FCS_Field is 0)
        zstdFrame.push_back(static_cast<std::uint8_t>(payloadSize & 0xFF));

        // Block Header: Last Block (bit 0 = 1), Block Type (bits 1-2 = 0 Raw), Block Size (bits 3-23)
        std::uint32_t blockHdr = 1 | (0 << 1) | (static_cast<std::uint32_t>(payloadSize) << 3);
        zstdFrame.push_back(static_cast<std::uint8_t>(blockHdr & 0xFF));
        zstdFrame.push_back(static_cast<std::uint8_t>((blockHdr >> 8) & 0xFF));
        zstdFrame.push_back(static_cast<std::uint8_t>((blockHdr >> 16) & 0xFF));

        // Payload
        for (char c : rawPayload) zstdFrame.push_back(static_cast<std::uint8_t>(c));

        // Inspect Frame
        auto info = CZSTDCodec::InspectFrame(zstdFrame);
        std::cout << "   ZSTD Valid Magic:    " << (info.m_bValid ? "TRUE" : "FALSE") << "\n";
        std::cout << "   Single Segment Mode: " << (info.m_bSingleSegment ? "TRUE" : "FALSE") << "\n";
        std::cout << "   Content Size:        " << info.m_nContentSize << " bytes\n";
        std::cout << "   Header Size:         " << info.m_nHeaderSize << " bytes\n";

        assert(info.m_bValid);
        assert(info.m_bSingleSegment);
        assert(info.m_nContentSize == payloadSize);

        // Decompress Raw Block Frame
        std::vector<std::uint8_t> decompressed(payloadSize, 0);
        bool bDecomp = CZSTDCodec::DecompressRawFrame(zstdFrame, decompressed);
        assert(bDecomp);
        std::string decompStr(reinterpret_cast<const char*>(decompressed.data()), decompressed.size());
        assert(decompStr == rawPayload);

        std::cout << "   [PASS] ZSTD frame metadata inspection and raw block extraction verified!\n";
        testsPassed++;
    }

    // =========================================================================
    // TEST 5: Source 2 Compiled Resource Container ("VASU") Unpacking
    // =========================================================================
    totalTests++;
    PrintTestHeader(5, "Source 2 Compiled Resource Container (\"VASU\") Processing");
    {
        // Build inner asset KV3 document
        CKV3Document modelDoc;
        modelDoc.m_root = KV3Table{};
        modelDoc.m_root["m_sModelPath"] = "characters/models/tm_phoenix.vmdl";
        modelDoc.m_root["m_nLODCount"] = 4;
        modelDoc.m_root["m_flBoundingRadius"] = 48.75;

        std::vector<std::uint8_t> vkPayload = modelDoc.SaveToBinary(KV3CompressionType::None);

        // Define external resource dependencies (RED2 block)
        std::vector<std::string> externalRefs = {
            "materials/models/characters/phoenix/tm_phoenix_body.vmat",
            "materials/models/characters/phoenix/tm_phoenix_head.vmat",
            "sounds/physics/footsteps/boots_concrete.vsnd"
        };

        // Build compiled container ("VASU")
        std::vector<std::uint8_t> containerBytes = CResourceFile::BuildContainer(vkPayload, externalRefs);

        std::cout << "   Generated Compiled Container Size: " << containerBytes.size() << " bytes\n";
        assert(containerBytes.size() >= sizeof(ResourceHeader_t));

        // Load via CResourceFile
        CResourceFile resFile;
        bool bLoaded = resFile.LoadFromMemory(containerBytes);
        assert(bLoaded);

        // Verify container header
        std::cout << "   Magic:          0x" << std::hex << resFile.m_header.m_nMagic << std::dec << " (\"VASU\")\n";
        std::cout << "   Header Version: " << resFile.m_header.m_nHeaderVersion << "\n";
        std::cout << "   Resource Ver:   " << resFile.m_header.m_nVersion << "\n";
        std::cout << "   Block Count:    " << resFile.m_header.m_nBlockCount << "\n";

        assert(resFile.m_header.m_nMagic == RESOURCE_MAGIC_VASU);
        assert(resFile.m_header.m_nHeaderVersion == 12);
        assert(resFile.m_header.m_nBlockCount == 2); // RED2 + DATA
        assert(resFile.HasBlock(BLOCK_FOURCC_RED2));
        assert(resFile.HasBlock(BLOCK_FOURCC_DATA));

        // Verify external dependencies
        std::cout << "   External Dependencies (" << resFile.m_externalReferences.size() << "):\n";
        for (const auto& ref : resFile.m_externalReferences) {
            std::cout << "     * " << ref.m_szAssetPath << "\n";
        }
        assert(resFile.m_externalReferences.size() == 3);
        assert(resFile.m_externalReferences[0].m_szAssetPath == "materials/models/characters/phoenix/tm_phoenix_body.vmat");

        // Unpack DATA block as KeyValues3 document
        CKV3Document unpackedDoc;
        bool bUnpacked = resFile.UnpackDataBlockAsKV3(unpackedDoc);
        assert(bUnpacked);

        assert(unpackedDoc.m_root["m_sModelPath"].AsString() == "characters/models/tm_phoenix.vmdl");
        assert(unpackedDoc.m_root["m_nLODCount"].AsInt() == 4);
        assert(std::abs(unpackedDoc.m_root["m_flBoundingRadius"].AsDouble() - 48.75) < 1e-4);

        std::cout << "   [PASS] Source 2 compiled container and embedded block extraction verified!\n";
        testsPassed++;
    }

    // =========================================================================
    // TEST 6: Schema-Driven Dynamic Object Mutation & Querying
    // =========================================================================
    totalTests++;
    PrintTestHeader(6, "Schema-Driven Dynamic Object Mutation & Querying");
    {
        CKV3Document doc;
        doc.m_root = KV3Table{};

        // Dynamic indexing & mutation
        doc.m_root["player"]["health"] = 100;
        doc.m_root["player"]["armor"] = 50;
        doc.m_root["player"]["inventory"][0] = "weapon_knife";
        doc.m_root["player"]["inventory"][1] = "weapon_ak47";
        doc.m_root["player"]["inventory"][2] = "weapon_smokegrenade";

        assert(doc.m_root["player"]["health"].AsInt() == 100);
        assert(doc.m_root["player"]["inventory"].Size() == 3);
        assert(doc.m_root["player"]["inventory"][1].AsString() == "weapon_ak47");

        // Test non-existent keys return defaults without crashing
        assert(doc.m_root["non_existent_key"].IsNull());
        assert(doc.m_root["non_existent_key"].AsInt(999) == 999);
        assert(doc.m_root["non_existent_key"].AsString("fallback") == "fallback");
        assert(doc.m_root["non_existent_key"].AsBool(true) == true);

        // Find query
        assert(doc.m_root.Find("player") != nullptr);
        assert(doc.m_root.Find("unknown_branch") == nullptr);
        assert(doc.m_root["player"].Find("armor") != nullptr);
        assert(doc.m_root["player"].Find("armor")->AsInt() == 50);

        std::cout << "   [PASS] Dynamic object graph mutation, deep traversal & null safety verified!\n";
        testsPassed++;
    }

    PrintSeparator('=');
    std::cout << " VERIFICATION RESULTS: " << testsPassed << " / " << totalTests
              << " Tests Passed (100% SUCCESS)\n";
    PrintSeparator('=');

    return 0;
}
