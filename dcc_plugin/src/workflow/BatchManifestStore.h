#pragma once

#include "MayaDmxWorkflow.h"

#include <filesystem>

namespace maya_dmx
{
class BatchManifestStore
{
public:
    bool ParseBatchManifestEntry(const MString &text, BatchManifestEntry &entry) const;

    MStatus SaveBatchManifest(const MString &name, const MStringArray &entries) const;
    MStatus LoadBatchManifest(const MString &name, MStringArray &entries) const;
    MStatus DeleteBatchManifest(const MString &name) const;
    MStatus ListBatchManifestNames(MStringArray &names) const;
    MStatus ListLegacyBatchManifestNames(MStringArray &names) const;
    MStatus MigrateLegacyBatchManifests(MStringArray &migratedNames) const;
    MStatus CleanupBatchManifestStorage(MStringArray &removedItems) const;

private:
    MStatus LoadLegacyBatchManifest(const MString &name, MStringArray &entries) const;
    MStatus ReadBatchManifestFile(const std::filesystem::path &manifestPath, MStringArray &entries) const;
};
}
