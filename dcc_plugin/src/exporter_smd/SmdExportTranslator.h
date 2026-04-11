#pragma once

#include <maya/MPxFileTranslator.h>

class SmdExportTranslator : public MPxFileTranslator
{
public:
    static void *Create();

    bool haveReadMethod() const override;
    bool haveWriteMethod() const override;
    MString defaultExtension() const override;
    MFileKind identifyFile(const MFileObject &fileObject, const char *buffer, short size) const override;
    MStatus writer(const MFileObject &fileObject, const MString &options, FileAccessMode mode) override;
};
