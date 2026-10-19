#include "schemasystem.h"

namespace cs2::schemasystem {

CSchemaSystem& CSchemaSystem::Get() {
    static CSchemaSystem instance;
    return instance;
}

CSchemaSystem::CSchemaSystem() = default;

void CSchemaSystem::RegisterClass(const std::string& moduleName, const std::string& className, int32_t classSize) {
    SchemaClass cls;
    cls.name = className;
    cls.moduleName = moduleName;
    cls.size = classSize;
    m_Classes[className] = std::move(cls);
}

void CSchemaSystem::RegisterField(const std::string& className, const std::string& fieldName, const std::string& type, int32_t offset) {
    auto it = m_Classes.find(className);
    if (it != m_Classes.end()) {
        it->second.fields[fieldName] = SchemaField{fieldName, type, offset};
    }
}

const SchemaClass* CSchemaSystem::FindDeclaredClass(const std::string& className) const {
    auto it = m_Classes.find(className);
    if (it != m_Classes.end()) {
        return &it->second;
    }
    return nullptr;
}

int32_t CSchemaSystem::GetFieldOffset(const std::string& className, const std::string& fieldName) const {
    const SchemaClass* cls = FindDeclaredClass(className);
    if (!cls) return -1;

    auto it = cls->fields.find(fieldName);
    if (it != cls->fields.end()) {
        return it->second.offset;
    }
    return -1;
}

} // namespace cs2::schemasystem
