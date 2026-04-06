#pragma once

#include <maya/MPxCommand.h>
#include <maya/MSyntax.h>

class MayaDmxWorkflowCommand : public MPxCommand
{
public:
    static void *Create();
    static MSyntax CreateSyntax();

    MStatus doIt(const MArgList &args) override;
    bool isUndoable() const override;
};
