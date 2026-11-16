#ifndef CS2_INTERFACES_FILE_SYSTEM_H
#define CS2_INTERFACES_FILE_SYSTEM_H

#include <cstdint>
#include <cstddef>

namespace cs2::interfaces {

/**
 * @brief Source 2 VPK & OS Virtual File System Interface (IFileSystem / CFileSystem_Stdio)
 * Token: "VFileSystem017" in filesystem_stdio.dll
 * VTable: filesystem_stdio.dll + 0x1b0ca0 (128 virtual slots)
 */
class IFileSystem {
public:
    // Slot 0: Virtual destructor
    virtual ~IFileSystem() = default;

    // Slot 1: IAppSystem::Connect
    virtual bool Connect(void* pFactory) = 0;

    // Slot 2: IAppSystem::Disconnect
    virtual void Disconnect() = 0;

    // Slot 3: IAppSystem::QueryInterface
    virtual void* QueryInterface(const char* pInterfaceName) = 0;

    // Slot 4: IAppSystem::Init
    virtual int Init() = 0;

    // Slot 5: IAppSystem::Shutdown
    virtual void Shutdown() = 0;

    // Virtual slots 6 to 10 (Internal / Unmapped)
    virtual void Unk06() = 0; virtual void Unk07() = 0; virtual void Unk08() = 0; virtual void Unk09() = 0;
    virtual void Unk10() = 0;

    // Slot 11: Reads raw bytes from open file handle
    virtual std::int32_t Read(void* pOutput, std::int32_t size, void* file) = 0;

    // Slot 12: Writes raw bytes to open file handle
    virtual std::int32_t Write(const void* pInput, std::int32_t size, void* file) = 0;

    // Slot 13: Opens file handle with access mode and pathID
    virtual void* Open(const char* pFileName, const char* pOptions, const char* pathID = nullptr) = 0;

    // Slot 14: Closes file handle
    virtual void Close(void* file) = 0;

    // Slot 15: Sets file stream read/write pointer
    virtual void Seek(void* file, std::int32_t pos, std::int32_t seekType) = 0;

    // Slot 16: Returns current file stream position
    virtual std::uint32_t Tell(void* file) = 0;

    // Slot 17: Returns open file length in bytes
    virtual std::uint32_t Size(void* file) = 0;

    // Slot 18: Returns file size by relative path
    virtual std::uint32_t SizeByName(const char* pFileName, const char* pPathID = nullptr) = 0;

    virtual void Unk19() = 0; // Slot 19
    virtual void Unk20() = 0; // Slot 20
    virtual void Unk21() = 0; // Slot 21

    // Slot 22: Tests file existence across search paths
    virtual bool FileExists(const char* pFileName, const char* pPathID = nullptr) = 0;

    // Slot 23: Tests directory path existence
    virtual bool IsDirectory(const char* pFileName, const char* pPathID = nullptr) = 0;

    // Virtual slots 24 to 31 (Internal / Unmapped)
    virtual void Unk24() = 0; virtual void Unk25() = 0; virtual void Unk26() = 0; virtual void Unk27() = 0;
    virtual void Unk28() = 0; virtual void Unk29() = 0; virtual void Unk30() = 0; virtual void Unk31() = 0;

    // Slot 32: Mounts directory or VPK archive into pathID
    virtual void AddSearchPath(const char* pPath, const char* pathID, int addType = 0) = 0;

    // Slot 33: Unmounts directory or archive from search paths
    virtual bool RemoveSearchPath(const char* pPath, const char* pathID) = 0;

    // Virtual slots 34 to 127 (Internal / Unmapped)
    virtual void Unk34() = 0; virtual void Unk35() = 0; virtual void Unk36() = 0; virtual void Unk37() = 0;
    virtual void Unk38() = 0; virtual void Unk39() = 0; virtual void Unk40() = 0; virtual void Unk41() = 0;
    virtual void Unk42() = 0; virtual void Unk43() = 0; virtual void Unk44() = 0; virtual void Unk45() = 0;
    virtual void Unk46() = 0; virtual void Unk47() = 0; virtual void Unk48() = 0; virtual void Unk49() = 0;
    virtual void Unk50() = 0; virtual void Unk51() = 0; virtual void Unk52() = 0; virtual void Unk53() = 0;
    virtual void Unk54() = 0; virtual void Unk55() = 0; virtual void Unk56() = 0; virtual void Unk57() = 0;
    virtual void Unk58() = 0; virtual void Unk59() = 0; virtual void Unk60() = 0; virtual void Unk61() = 0;
    virtual void Unk62() = 0; virtual void Unk63() = 0; virtual void Unk64() = 0; virtual void Unk65() = 0;
    virtual void Unk66() = 0; virtual void Unk67() = 0; virtual void Unk68() = 0; virtual void Unk69() = 0;
    virtual void Unk70() = 0; virtual void Unk71() = 0; virtual void Unk72() = 0; virtual void Unk73() = 0;
    virtual void Unk74() = 0; virtual void Unk75() = 0; virtual void Unk76() = 0; virtual void Unk77() = 0;
    virtual void Unk78() = 0; virtual void Unk79() = 0; virtual void Unk80() = 0; virtual void Unk81() = 0;
    virtual void Unk82() = 0; virtual void Unk83() = 0; virtual void Unk84() = 0; virtual void Unk85() = 0;
    virtual void Unk86() = 0; virtual void Unk87() = 0; virtual void Unk88() = 0; virtual void Unk89() = 0;
    virtual void Unk90() = 0; virtual void Unk91() = 0; virtual void Unk92() = 0; virtual void Unk93() = 0;
    virtual void Unk94() = 0; virtual void Unk95() = 0; virtual void Unk96() = 0; virtual void Unk97() = 0;
    virtual void Unk98() = 0; virtual void Unk99() = 0; virtual void Unk100() = 0; virtual void Unk101() = 0;
    virtual void Unk102() = 0; virtual void Unk103() = 0; virtual void Unk104() = 0; virtual void Unk105() = 0;
    virtual void Unk106() = 0; virtual void Unk107() = 0; virtual void Unk108() = 0; virtual void Unk109() = 0;
    virtual void Unk110() = 0; virtual void Unk111() = 0; virtual void Unk112() = 0; virtual void Unk113() = 0;
    virtual void Unk114() = 0; virtual void Unk115() = 0; virtual void Unk116() = 0; virtual void Unk117() = 0;
    virtual void Unk118() = 0; virtual void Unk119() = 0; virtual void Unk120() = 0; virtual void Unk121() = 0;
    virtual void Unk122() = 0; virtual void Unk123() = 0; virtual void Unk124() = 0; virtual void Unk125() = 0;
    virtual void Unk126() = 0; virtual void Unk127() = 0;

};
static_assert(sizeof(IFileSystem) == 0x08, "IFileSystem interface pointer must be 8 bytes");

using CFileSystem_Stdio = IFileSystem;

} // namespace cs2::interfaces

#endif // CS2_INTERFACES_FILE_SYSTEM_H
