#pragma once
#include "LaceGarmentDocument.h"
namespace LaceGarment
{
enum class ETemplate : uint8 { TShirt, DolphinShorts };
// Replaces template source on explicit creation/parameter regeneration; preserves stable IDs for the same PatternId.
// All editable piece definitions are embedded in Document. No external template is needed to reopen it.
LACEGARMENTEDITOR_API bool ApplyTemplate(ULaceGarmentDocument& Document,ETemplate Template,const TMap<FString,double>& ParametersMm,FString& Error);
}
