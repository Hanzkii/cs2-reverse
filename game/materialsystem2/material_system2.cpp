#include "material_system2.h"

namespace cs2::materialsystem2 {

// Emulated material descriptor for tooling & standalone tests
class CMaterial2Standalone : public CMaterial2 {
public:
    const char* m_pName{"materials/models/player/ctm_sas.vmat"};
    const char* m_pShader{"csgo_character.vfx"};

    const char* GetName() const override { return m_pName; }
    void GetPath(void* /*pBufferString*/) const override {}
    const char* GetShaderName() const override { return m_pShader; }
    std::uint8_t GetFlags() const override { return 0; }
    void* GetMaterialData() const override { return nullptr; }
    std::int32_t GetMaterialId() const override { return 1; }
    void* GetShaderBinding() override { return nullptr; }
    void IncrementRefCount() override {}
};

static CMaterial2Standalone s_DefaultMaterial;

} // namespace cs2::materialsystem2
