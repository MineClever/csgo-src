#pragma once

#include <maya/MPxFileTranslator.h>

class ObjImportTranslator : public MPxFileTranslator
{
public:
    static void *Create();

    bool haveReadMethod() const override;
    bool haveWriteMethod() const override;
    bool haveNamespaceSupport() const override;
    bool canBeOpened() const override;
    MString defaultExtension() const override;
    MFileKind identifyFile(const MFileObject &fileObject, const char *buffer, short size) const override;
    MStatus reader(const MFileObject &fileObject, const MString &options, FileAccessMode mode) override;
};
