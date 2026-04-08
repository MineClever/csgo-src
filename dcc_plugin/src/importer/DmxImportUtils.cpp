#include "DmxImportUtils.h"

namespace dmx_import_utils
{
std::string SanitizeNodeName(const std::string &name)
{
    std::string sanitized = name.empty() ? "dmxMaterial" : name;
    for (char &ch : sanitized)
    {
        const bool ok = (ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '_';
        if (!ok)
        {
            ch = '_';
        }
    }
    return sanitized;
}
}
