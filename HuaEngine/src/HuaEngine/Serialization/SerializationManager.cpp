#include "enginepch.h"
#include "SerializationManager.h"

namespace HE::Serialization {

    void SerializationManager::RegisterBackend(SerializationFormat format, 
                                             std::function<std::unique_ptr<SerializationBackend>()> factory) {
        m_Backends[format] = factory;
    }

    std::unique_ptr<SerializationBackend> SerializationManager::CreateBackend(SerializationFormat format) {
        auto it = m_Backends.find(format);
        if (it != m_Backends.end()) {
            return it->second();
        }
        return nullptr;
    }

} // namespace HE::Serialization
