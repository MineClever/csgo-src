#include "DmxImportTranslator.h"
#include "DmxImportInternals.h"
#include "DmxImportSession.h"

#include <common_dmx/MayaDmxCommon.h>

#include <string>

void *DmxImportTranslator::Create()
{
    return new DmxImportTranslator();
}

bool DmxImportTranslator::haveReadMethod() const
{
    return true;
}

bool DmxImportTranslator::haveWriteMethod() const
{
    return false;
}

bool DmxImportTranslator::haveNamespaceSupport() const
{
    return true;
}

bool DmxImportTranslator::canBeOpened() const
{
    return true;
}

MString DmxImportTranslator::defaultExtension() const
{
    return "dmx";
}

MPxFileTranslator::MFileKind DmxImportTranslator::identifyFile(const MFileObject &fileObject, const char *, short) const
{
    return maya_dmx::HasDmxExtension(fileObject) ? kIsMyFileType : kNotMyFileType;
}

MStatus DmxImportTranslator::reader(const MFileObject &fileObject, const MString &options, FileAccessMode)
{
    dmx_import_impl::ResetImportDebugLog();
    dmx_import_impl::AppendImportDebugLog("translator: reader begin");
    dmx_import_impl::AppendImportDebugLog((std::string("translator: file=") + fileObject.rawFullName().asChar()).c_str());
    dmx_import_impl::AppendImportDebugLog((std::string("translator: options=") + options.asChar()).c_str());
    DmxImportSession session(fileObject, options);
    const MStatus status = session.Run();
    dmx_import_impl::AppendImportDebugLog((std::string("translator: reader end status=") + (status ? "success" : "failure")).c_str());
    return status;
}

