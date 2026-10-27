#ifndef CS2_MATERIALSYSTEM2_H
#define CS2_MATERIALSYSTEM2_H

#include <cstdint>
#include <cstddef>

namespace cs2::materialsystem2 {

/**
 * @brief Material Instance Runtime Interface (CMaterial2)
 * VTable: materialsystem2.dll + 0x129ae0 (25 methods)
 */
class CMaterial2 {
public:
    virtual ~CMaterial2() = default;

    // Slot 1: GetName (RVA: +0x0000b350)
    virtual const char* GetName() const = 0;

    // Slot 2: GetPath (RVA: +0x0000b370)
    virtual void GetPath(void* pBufferString) const = 0;

    // Slot 3: GetShaderName (RVA: +0x0000b5e0)
    virtual const char* GetShaderName() const = 0;

    // Slot 4: GetFlags (RVA: +0x0000a100)
    virtual std::uint8_t GetFlags() const = 0;

    // Slot 7: GetMaterialData (RVA: +0x0000ba60)
    virtual void* GetMaterialData() const = 0;

    // Slot 25: GetMaterialId (RVA: +0x0000b880)
    virtual std::int32_t GetMaterialId() const = 0;

    // Slot 26: GetShaderBinding (RVA: +0x0001b2d0)
    virtual void* GetShaderBinding() = 0;

    // Slot 29: IncrementRefCount (RVA: +0x0000b890)
    virtual void IncrementRefCount() = 0;
};
static_assert(sizeof(CMaterial2) == 8, "CMaterial2 interface must be 8 bytes (vptr)");

/**
 * @brief Source 2 Material System Interface (IMaterialSystem2)
 * Token: "VMaterialSystem2_001" in materialsystem2.dll
 * VTable: +0x12d920 (57 slots)
 */
class IMaterialSystem2 {
public:
    virtual ~IMaterialSystem2() = default;

    // Slot 14: FindOrCreateMaterialFromResource
    virtual CMaterial2* FindOrCreateMaterialFromResource(const char* pMaterialResourceName) = 0;

    // Slot 24: CreateMaterial
    virtual CMaterial2* CreateMaterial(const char* pMaterialName, void* pKeyValues) = 0;
};

} // namespace cs2::materialsystem2

#endif // CS2_MATERIALSYSTEM2_H
