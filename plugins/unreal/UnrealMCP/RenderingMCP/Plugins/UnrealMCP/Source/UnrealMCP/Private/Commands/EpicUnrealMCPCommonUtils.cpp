#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "GameFramework/Actor.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_InputAction.h"
#include "K2Node_Self.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "UObject/UObjectIterator.h"
#include "Engine/Selection.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabase.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"

namespace
{
constexpr int32 GMCPMaxCollectionEntries = 256;

bool ShouldSerializeProperty(const FProperty* Property, const bool bIncludeAllProperties)
{
    return Property
        && !Property->HasAnyPropertyFlags(CPF_Deprecated)
        && (bIncludeAllProperties || Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible));
}

FString ExportPropertyValueToString(const FProperty* Property, const void* PropertyAddr)
{
    FString ExportedValue;
    if (Property && PropertyAddr)
    {
        Property->ExportTextItem_Direct(ExportedValue, PropertyAddr, nullptr, nullptr, PPF_None);
    }
    return ExportedValue;
}

TSharedPtr<FJsonObject> BuildObjectSummary(UObject* Object)
{
    TSharedPtr<FJsonObject> ObjectJson = MakeShared<FJsonObject>();
    ObjectJson->SetBoolField(TEXT("valid"), Object != nullptr);
    if (!Object)
    {
        return ObjectJson;
    }

    ObjectJson->SetStringField(TEXT("object_name"), Object->GetName());
    ObjectJson->SetStringField(TEXT("object_path"), Object->GetPathName());
    ObjectJson->SetStringField(TEXT("object_class"), Object->GetClass()->GetName());
    ObjectJson->SetStringField(TEXT("object_class_path"), Object->GetClass()->GetPathName());
    if (UObject* Outer = Object->GetOuter())
    {
        ObjectJson->SetStringField(TEXT("outer_path"), Outer->GetPathName());
    }

    return ObjectJson;
}

TSharedPtr<FJsonValue> SerializePropertyValue(const FProperty* Property, const void* PropertyAddr,
    int32 MaxDepth, bool bIncludeAllProperties, TSet<const UObject*>& VisitedObjects);

TSharedPtr<FJsonObject> SerializeStructProperties(const UStruct* StructType, const void* StructData,
    int32 MaxDepth, bool bIncludeAllProperties, TSet<const UObject*>& VisitedObjects)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    if (!StructType || !StructData)
    {
        return Result;
    }

    Result->SetStringField(TEXT("__type"), StructType->GetName());

    for (TFieldIterator<FProperty> PropIt(StructType, EFieldIterationFlags::IncludeSuper); PropIt; ++PropIt)
    {
        const FProperty* Property = *PropIt;
        if (!ShouldSerializeProperty(Property, bIncludeAllProperties))
        {
            continue;
        }

        const void* ValuePtr = Property->ContainerPtrToValuePtr<const void>(StructData);
        TSharedPtr<FJsonValue> JsonValue = SerializePropertyValue(Property, ValuePtr, MaxDepth, bIncludeAllProperties, VisitedObjects);
        if (JsonValue.IsValid() && !JsonValue->IsNull())
        {
            Result->SetField(Property->GetName(), JsonValue);
        }
    }

    return Result;
}

TSharedPtr<FJsonValue> SerializeNumericProperty(const FNumericProperty* NumericProperty, const void* PropertyAddr)
{
    if (!NumericProperty || !PropertyAddr)
    {
        return MakeShared<FJsonValueNull>();
    }

    if (NumericProperty->IsInteger())
    {
        return MakeShared<FJsonValueNumber>(static_cast<double>(NumericProperty->GetSignedIntPropertyValue(PropertyAddr)));
    }

    return MakeShared<FJsonValueNumber>(NumericProperty->GetFloatingPointPropertyValue(PropertyAddr));
}

TSharedPtr<FJsonValue> SerializeEnumValue(const UEnum* EnumDef, const int64 EnumValue)
{
    TSharedPtr<FJsonObject> EnumJson = MakeShared<FJsonObject>();
    EnumJson->SetNumberField(TEXT("value"), static_cast<double>(EnumValue));
    if (EnumDef)
    {
        EnumJson->SetStringField(TEXT("enum_type"), EnumDef->GetName());
        EnumJson->SetStringField(TEXT("enum_name"), EnumDef->GetNameStringByValue(EnumValue));
    }
    return MakeShared<FJsonValueObject>(EnumJson);
}

TSharedPtr<FJsonValue> SerializeStructValue(const FStructProperty* StructProperty, const void* PropertyAddr,
    int32 MaxDepth, bool bIncludeAllProperties, TSet<const UObject*>& VisitedObjects)
{
    if (!StructProperty || !PropertyAddr)
    {
        return MakeShared<FJsonValueNull>();
    }

    const UScriptStruct* Struct = StructProperty->Struct;
    if (!Struct)
    {
        return MakeShared<FJsonValueNull>();
    }

    static const FName Vector3fStructName(TEXT("Vector3f"));
    static const FName Vector4fStructName(TEXT("Vector4f"));

    if (Struct == TBaseStructure<FVector>::Get())
    {
        const FVector* Vector = static_cast<const FVector*>(PropertyAddr);
        TArray<TSharedPtr<FJsonValue>> Values;
        Values.Add(MakeShared<FJsonValueNumber>(Vector->X));
        Values.Add(MakeShared<FJsonValueNumber>(Vector->Y));
        Values.Add(MakeShared<FJsonValueNumber>(Vector->Z));
        return MakeShared<FJsonValueArray>(Values);
    }
    if (Struct == TBaseStructure<FVector2D>::Get())
    {
        const FVector2D* Vector = static_cast<const FVector2D*>(PropertyAddr);
        TArray<TSharedPtr<FJsonValue>> Values;
        Values.Add(MakeShared<FJsonValueNumber>(Vector->X));
        Values.Add(MakeShared<FJsonValueNumber>(Vector->Y));
        return MakeShared<FJsonValueArray>(Values);
    }
    if (Struct == TBaseStructure<FVector4>::Get())
    {
        const FVector4* Vector = static_cast<const FVector4*>(PropertyAddr);
        TArray<TSharedPtr<FJsonValue>> Values;
        Values.Add(MakeShared<FJsonValueNumber>(Vector->X));
        Values.Add(MakeShared<FJsonValueNumber>(Vector->Y));
        Values.Add(MakeShared<FJsonValueNumber>(Vector->Z));
        Values.Add(MakeShared<FJsonValueNumber>(Vector->W));
        return MakeShared<FJsonValueArray>(Values);
    }
    if (Struct->GetFName() == Vector3fStructName)
    {
        const FVector3f* Vector = static_cast<const FVector3f*>(PropertyAddr);
        TArray<TSharedPtr<FJsonValue>> Values;
        Values.Add(MakeShared<FJsonValueNumber>(Vector->X));
        Values.Add(MakeShared<FJsonValueNumber>(Vector->Y));
        Values.Add(MakeShared<FJsonValueNumber>(Vector->Z));
        return MakeShared<FJsonValueArray>(Values);
    }
    if (Struct->GetFName() == Vector4fStructName)
    {
        const FVector4f* Vector = static_cast<const FVector4f*>(PropertyAddr);
        TArray<TSharedPtr<FJsonValue>> Values;
        Values.Add(MakeShared<FJsonValueNumber>(Vector->X));
        Values.Add(MakeShared<FJsonValueNumber>(Vector->Y));
        Values.Add(MakeShared<FJsonValueNumber>(Vector->Z));
        Values.Add(MakeShared<FJsonValueNumber>(Vector->W));
        return MakeShared<FJsonValueArray>(Values);
    }
    if (Struct == TBaseStructure<FRotator>::Get())
    {
        const FRotator* Rotator = static_cast<const FRotator*>(PropertyAddr);
        TArray<TSharedPtr<FJsonValue>> Values;
        Values.Add(MakeShared<FJsonValueNumber>(Rotator->Pitch));
        Values.Add(MakeShared<FJsonValueNumber>(Rotator->Yaw));
        Values.Add(MakeShared<FJsonValueNumber>(Rotator->Roll));
        return MakeShared<FJsonValueArray>(Values);
    }
    if (Struct == TBaseStructure<FQuat>::Get())
    {
        const FQuat* Quat = static_cast<const FQuat*>(PropertyAddr);
        TArray<TSharedPtr<FJsonValue>> Values;
        Values.Add(MakeShared<FJsonValueNumber>(Quat->X));
        Values.Add(MakeShared<FJsonValueNumber>(Quat->Y));
        Values.Add(MakeShared<FJsonValueNumber>(Quat->Z));
        Values.Add(MakeShared<FJsonValueNumber>(Quat->W));
        return MakeShared<FJsonValueArray>(Values);
    }
    if (Struct == TBaseStructure<FTransform>::Get())
    {
        const FTransform* Transform = static_cast<const FTransform*>(PropertyAddr);
        const FVector Translation = Transform->GetTranslation();
        const FQuat Rotation = Transform->GetRotation();
        const FVector Scale3D = Transform->GetScale3D();
        TSharedPtr<FJsonObject> TransformJson = MakeShared<FJsonObject>();

        TArray<TSharedPtr<FJsonValue>> TranslationValues;
        TranslationValues.Add(MakeShared<FJsonValueNumber>(Translation.X));
        TranslationValues.Add(MakeShared<FJsonValueNumber>(Translation.Y));
        TranslationValues.Add(MakeShared<FJsonValueNumber>(Translation.Z));
        TransformJson->SetArrayField(TEXT("translation"), TranslationValues);

        TArray<TSharedPtr<FJsonValue>> RotationValues;
        RotationValues.Add(MakeShared<FJsonValueNumber>(Rotation.X));
        RotationValues.Add(MakeShared<FJsonValueNumber>(Rotation.Y));
        RotationValues.Add(MakeShared<FJsonValueNumber>(Rotation.Z));
        RotationValues.Add(MakeShared<FJsonValueNumber>(Rotation.W));
        TransformJson->SetArrayField(TEXT("rotation"), RotationValues);

        TArray<TSharedPtr<FJsonValue>> ScaleValues;
        ScaleValues.Add(MakeShared<FJsonValueNumber>(Scale3D.X));
        ScaleValues.Add(MakeShared<FJsonValueNumber>(Scale3D.Y));
        ScaleValues.Add(MakeShared<FJsonValueNumber>(Scale3D.Z));
        TransformJson->SetArrayField(TEXT("scale"), ScaleValues);

        return MakeShared<FJsonValueObject>(TransformJson);
    }
    if (Struct == TBaseStructure<FLinearColor>::Get())
    {
        const FLinearColor* Color = static_cast<const FLinearColor*>(PropertyAddr);
        TArray<TSharedPtr<FJsonValue>> Values;
        Values.Add(MakeShared<FJsonValueNumber>(Color->R));
        Values.Add(MakeShared<FJsonValueNumber>(Color->G));
        Values.Add(MakeShared<FJsonValueNumber>(Color->B));
        Values.Add(MakeShared<FJsonValueNumber>(Color->A));
        return MakeShared<FJsonValueArray>(Values);
    }
    if (Struct == TBaseStructure<FColor>::Get())
    {
        const FColor* Color = static_cast<const FColor*>(PropertyAddr);
        TArray<TSharedPtr<FJsonValue>> Values;
        Values.Add(MakeShared<FJsonValueNumber>(Color->R));
        Values.Add(MakeShared<FJsonValueNumber>(Color->G));
        Values.Add(MakeShared<FJsonValueNumber>(Color->B));
        Values.Add(MakeShared<FJsonValueNumber>(Color->A));
        return MakeShared<FJsonValueArray>(Values);
    }
    if (Struct == TBaseStructure<FGuid>::Get())
    {
        const FGuid* Guid = static_cast<const FGuid*>(PropertyAddr);
        return MakeShared<FJsonValueString>(Guid->ToString());
    }

    if (MaxDepth <= 0)
    {
        return MakeShared<FJsonValueString>(ExportPropertyValueToString(StructProperty, PropertyAddr));
    }

    return MakeShared<FJsonValueObject>(SerializeStructProperties(Struct, PropertyAddr, MaxDepth - 1, bIncludeAllProperties, VisitedObjects));
}

TSharedPtr<FJsonValue> SerializePropertyValue(const FProperty* Property, const void* PropertyAddr,
    int32 MaxDepth, bool bIncludeAllProperties, TSet<const UObject*>& VisitedObjects)
{
    if (!Property || !PropertyAddr)
    {
        return MakeShared<FJsonValueNull>();
    }

    if (const FBoolProperty* BoolProperty = CastField<const FBoolProperty>(Property))
    {
        return MakeShared<FJsonValueBoolean>(BoolProperty->GetPropertyValue(PropertyAddr));
    }
    if (const FEnumProperty* EnumProperty = CastField<const FEnumProperty>(Property))
    {
        const int64 EnumValue = EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(PropertyAddr);
        return SerializeEnumValue(EnumProperty->GetEnum(), EnumValue);
    }
    if (const FByteProperty* ByteProperty = CastField<const FByteProperty>(Property))
    {
        if (ByteProperty->GetIntPropertyEnum())
        {
            return SerializeEnumValue(ByteProperty->GetIntPropertyEnum(), ByteProperty->GetPropertyValue(PropertyAddr));
        }
        return MakeShared<FJsonValueNumber>(ByteProperty->GetPropertyValue(PropertyAddr));
    }
    if (const FNumericProperty* NumericProperty = CastField<const FNumericProperty>(Property))
    {
        return SerializeNumericProperty(NumericProperty, PropertyAddr);
    }
    if (const FStrProperty* StrProperty = CastField<const FStrProperty>(Property))
    {
        return MakeShared<FJsonValueString>(StrProperty->GetPropertyValue(PropertyAddr));
    }
    if (const FNameProperty* NameProperty = CastField<const FNameProperty>(Property))
    {
        return MakeShared<FJsonValueString>(NameProperty->GetPropertyValue(PropertyAddr).ToString());
    }
    if (const FTextProperty* TextProperty = CastField<const FTextProperty>(Property))
    {
        return MakeShared<FJsonValueString>(TextProperty->GetPropertyValue(PropertyAddr).ToString());
    }
    if (const FStructProperty* StructProperty = CastField<const FStructProperty>(Property))
    {
        return SerializeStructValue(StructProperty, PropertyAddr, MaxDepth, bIncludeAllProperties, VisitedObjects);
    }
    if (const FObjectPropertyBase* ObjectProperty = CastField<const FObjectPropertyBase>(Property))
    {
        UObject* ReferencedObject = ObjectProperty->GetObjectPropertyValue(PropertyAddr);
        if (!ReferencedObject)
        {
            return MakeShared<FJsonValueNull>();
        }

        TSharedPtr<FJsonObject> ObjectJson = BuildObjectSummary(ReferencedObject);
        if (MaxDepth > 0 && !VisitedObjects.Contains(ReferencedObject))
        {
            VisitedObjects.Add(ReferencedObject);
            ObjectJson->SetObjectField(TEXT("properties"), SerializeStructProperties(ReferencedObject->GetClass(), ReferencedObject, MaxDepth - 1, bIncludeAllProperties, VisitedObjects));
            VisitedObjects.Remove(ReferencedObject);
        }
        return MakeShared<FJsonValueObject>(ObjectJson);
    }
    if (const FSoftObjectProperty* SoftObjectProperty = CastField<const FSoftObjectProperty>(Property))
    {
        const FSoftObjectPtr SoftObject = SoftObjectProperty->GetPropertyValue(PropertyAddr);
        if (SoftObject.IsNull())
        {
            return MakeShared<FJsonValueNull>();
        }

        TSharedPtr<FJsonObject> ObjectJson = MakeShared<FJsonObject>();
        ObjectJson->SetStringField(TEXT("soft_object_path"), SoftObject.ToSoftObjectPath().ToString());
        ObjectJson->SetBoolField(TEXT("is_loaded"), SoftObject.IsValid());
        return MakeShared<FJsonValueObject>(ObjectJson);
    }
    if (const FSoftClassProperty* SoftClassProperty = CastField<const FSoftClassProperty>(Property))
    {
        const FSoftObjectPtr SoftClass = SoftClassProperty->GetPropertyValue(PropertyAddr);
        if (SoftClass.IsNull())
        {
            return MakeShared<FJsonValueNull>();
        }

        TSharedPtr<FJsonObject> ClassJson = MakeShared<FJsonObject>();
        ClassJson->SetStringField(TEXT("soft_class_path"), SoftClass.ToSoftObjectPath().ToString());
        ClassJson->SetBoolField(TEXT("is_loaded"), SoftClass.IsValid());
        return MakeShared<FJsonValueObject>(ClassJson);
    }
    if (const FClassProperty* ClassProperty = CastField<const FClassProperty>(Property))
    {
        UClass* ClassValue = Cast<UClass>(ClassProperty->GetObjectPropertyValue(PropertyAddr));
        if (!ClassValue)
        {
            return MakeShared<FJsonValueNull>();
        }

        TSharedPtr<FJsonObject> ClassJson = MakeShared<FJsonObject>();
        ClassJson->SetStringField(TEXT("class_name"), ClassValue->GetName());
        ClassJson->SetStringField(TEXT("class_path"), ClassValue->GetPathName());
        return MakeShared<FJsonValueObject>(ClassJson);
    }
    if (const FArrayProperty* ArrayProperty = CastField<const FArrayProperty>(Property))
    {
        FScriptArrayHelper ArrayHelper(ArrayProperty, PropertyAddr);
        TArray<TSharedPtr<FJsonValue>> Items;
        const int32 EntryCount = FMath::Min(ArrayHelper.Num(), GMCPMaxCollectionEntries);
        for (int32 Index = 0; Index < EntryCount; ++Index)
        {
            Items.Add(SerializePropertyValue(ArrayProperty->Inner, ArrayHelper.GetRawPtr(Index), MaxDepth, bIncludeAllProperties, VisitedObjects));
        }
        return MakeShared<FJsonValueArray>(Items);
    }
    if (const FSetProperty* SetProperty = CastField<const FSetProperty>(Property))
    {
        FScriptSetHelper SetHelper(SetProperty, PropertyAddr);
        TArray<TSharedPtr<FJsonValue>> Items;
        int32 AddedCount = 0;
        for (int32 Index = 0; Index < SetHelper.GetMaxIndex() && AddedCount < GMCPMaxCollectionEntries; ++Index)
        {
            if (!SetHelper.IsValidIndex(Index))
            {
                continue;
            }
            Items.Add(SerializePropertyValue(SetProperty->ElementProp, SetHelper.GetElementPtr(Index), MaxDepth, bIncludeAllProperties, VisitedObjects));
            ++AddedCount;
        }
        return MakeShared<FJsonValueArray>(Items);
    }
    if (const FMapProperty* MapProperty = CastField<const FMapProperty>(Property))
    {
        FScriptMapHelper MapHelper(MapProperty, PropertyAddr);
        TArray<TSharedPtr<FJsonValue>> Entries;
        int32 AddedCount = 0;
        for (int32 Index = 0; Index < MapHelper.GetMaxIndex() && AddedCount < GMCPMaxCollectionEntries; ++Index)
        {
            if (!MapHelper.IsValidIndex(Index))
            {
                continue;
            }

            TSharedPtr<FJsonObject> EntryJson = MakeShared<FJsonObject>();
            EntryJson->SetField(TEXT("key"), SerializePropertyValue(MapProperty->KeyProp, MapHelper.GetKeyPtr(Index), MaxDepth, bIncludeAllProperties, VisitedObjects));
            EntryJson->SetField(TEXT("value"), SerializePropertyValue(MapProperty->ValueProp, MapHelper.GetValuePtr(Index), MaxDepth, bIncludeAllProperties, VisitedObjects));
            Entries.Add(MakeShared<FJsonValueObject>(EntryJson));
            ++AddedCount;
        }
        return MakeShared<FJsonValueArray>(Entries);
    }

    return MakeShared<FJsonValueString>(ExportPropertyValueToString(Property, PropertyAddr));
}
}

// JSON Utilities
TSharedPtr<FJsonObject> FEpicUnrealMCPCommonUtils::CreateErrorResponse(const FString& Message)
{
    TSharedPtr<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
    ResponseObject->SetBoolField(TEXT("success"), false);
    ResponseObject->SetStringField(TEXT("error"), Message);
    return ResponseObject;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPCommonUtils::CreateSuccessResponse(const TSharedPtr<FJsonObject>& Data)
{
    TSharedPtr<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
    ResponseObject->SetBoolField(TEXT("success"), true);
    
    if (Data.IsValid())
    {
        ResponseObject->SetObjectField(TEXT("data"), Data);
    }
    
    return ResponseObject;
}

void FEpicUnrealMCPCommonUtils::GetIntArrayFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<int32>& OutArray)
{
    OutArray.Reset();
    
    if (!JsonObject->HasField(FieldName))
    {
        return;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray))
    {
        for (const TSharedPtr<FJsonValue>& Value : *JsonArray)
        {
            OutArray.Add((int32)Value->AsNumber());
        }
    }
}

void FEpicUnrealMCPCommonUtils::GetFloatArrayFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<float>& OutArray)
{
    OutArray.Reset();
    
    if (!JsonObject->HasField(FieldName))
    {
        return;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray))
    {
        for (const TSharedPtr<FJsonValue>& Value : *JsonArray)
        {
            OutArray.Add((float)Value->AsNumber());
        }
    }
}

FVector2D FEpicUnrealMCPCommonUtils::GetVector2DFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
{
    FVector2D Result(0.0f, 0.0f);
    
    if (!JsonObject->HasField(FieldName))
    {
        return Result;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray) && JsonArray->Num() >= 2)
    {
        Result.X = (float)(*JsonArray)[0]->AsNumber();
        Result.Y = (float)(*JsonArray)[1]->AsNumber();
    }
    
    return Result;
}

FVector FEpicUnrealMCPCommonUtils::GetVectorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
{
    FVector Result(0.0f, 0.0f, 0.0f);
    
    if (!JsonObject->HasField(FieldName))
    {
        return Result;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray) && JsonArray->Num() >= 3)
    {
        Result.X = (float)(*JsonArray)[0]->AsNumber();
        Result.Y = (float)(*JsonArray)[1]->AsNumber();
        Result.Z = (float)(*JsonArray)[2]->AsNumber();
    }
    
    return Result;
}

FRotator FEpicUnrealMCPCommonUtils::GetRotatorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
{
    FRotator Result(0.0f, 0.0f, 0.0f);
    
    if (!JsonObject->HasField(FieldName))
    {
        return Result;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray) && JsonArray->Num() >= 3)
    {
        Result.Pitch = (float)(*JsonArray)[0]->AsNumber();
        Result.Yaw = (float)(*JsonArray)[1]->AsNumber();
        Result.Roll = (float)(*JsonArray)[2]->AsNumber();
    }
    
    return Result;
}

// Blueprint Utilities
UBlueprint* FEpicUnrealMCPCommonUtils::FindBlueprint(const FString& BlueprintName)
{
    return FindBlueprintByName(BlueprintName);
}

UBlueprint* FEpicUnrealMCPCommonUtils::FindBlueprintByName(const FString& BlueprintName)
{
    // The correct object path for a Blueprint asset is /Game/Path/AssetName.AssetName
    FString ObjectPath;

    // Check if BlueprintName is already a full path (starts with /)
    if (BlueprintName.StartsWith(TEXT("/")))
    {
        // It's already a full path, use it directly with the class suffix
        FString AssetName = FPaths::GetBaseFilename(BlueprintName);
        ObjectPath = FString::Printf(TEXT("%s.%s"), *BlueprintName, *AssetName);
    }
    else
    {
        // It's just a name, add the default /Game/Blueprints/ prefix
        ObjectPath = FString::Printf(TEXT("/Game/Blueprints/%s.%s"), *BlueprintName, *BlueprintName);
    }

    // First, try to load the object directly, as it's the fastest method.
    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath);
    if (Blueprint)
    {
        return Blueprint;
    }

    // If direct loading fails, try to find the asset using the Asset Registry.
    // This is more robust for newly created assets.
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(ObjectPath));

    if (AssetData.IsValid())
    {
        Blueprint = Cast<UBlueprint>(AssetData.GetAsset());
        if (Blueprint)
        {
            return Blueprint;
        }
    }

    // Fallback for cases where the asset is in memory but not yet fully saved,
    // where it might be found via its package path.
    FString PackagePath = TEXT("/Game/Blueprints/") + BlueprintName;
    Blueprint = FindObject<UBlueprint>(nullptr, *PackagePath);

    if (!Blueprint)
    {
         UE_LOG(LogTemp, Error, TEXT("FindBlueprintByName: Failed to find or load blueprint: %s"), *BlueprintName);
    }

    return Blueprint;
}

UEdGraph* FEpicUnrealMCPCommonUtils::FindOrCreateEventGraph(UBlueprint* Blueprint)
{
    if (!Blueprint)
    {
        return nullptr;
    }
    
    // Try to find the event graph
    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (Graph->GetName().Contains(TEXT("EventGraph")))
        {
            return Graph;
        }
    }
    
    // Create a new event graph if none exists
    UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, FName(TEXT("EventGraph")), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(Blueprint, NewGraph);
    return NewGraph;
}

// Blueprint node utilities
UK2Node_Event* FEpicUnrealMCPCommonUtils::CreateEventNode(UEdGraph* Graph, const FString& EventName, const FVector2D& Position)
{
    if (!Graph)
    {
        return nullptr;
    }
    
    UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
    if (!Blueprint)
    {
        return nullptr;
    }
    
    // Check for existing event node with this exact name
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
        if (EventNode && EventNode->EventReference.GetMemberName() == FName(*EventName))
        {
            UE_LOG(LogTemp, Display, TEXT("Using existing event node with name %s (ID: %s)"), 
                *EventName, *EventNode->NodeGuid.ToString());
            return EventNode;
        }
    }

    // No existing node found, create a new one
    UK2Node_Event* EventNode = nullptr;
    
    // Find the function to create the event
    UClass* BlueprintClass = Blueprint->GeneratedClass;
    UFunction* EventFunction = BlueprintClass->FindFunctionByName(FName(*EventName));
    
    if (EventFunction)
    {
        EventNode = NewObject<UK2Node_Event>(Graph);
        EventNode->EventReference.SetExternalMember(FName(*EventName), BlueprintClass);
        EventNode->NodePosX = Position.X;
        EventNode->NodePosY = Position.Y;
        Graph->AddNode(EventNode, true);
        EventNode->PostPlacedNewNode();
        EventNode->AllocateDefaultPins();
        UE_LOG(LogTemp, Display, TEXT("Created new event node with name %s (ID: %s)"), 
            *EventName, *EventNode->NodeGuid.ToString());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to find function for event name: %s"), *EventName);
    }
    
    return EventNode;
}

UK2Node_CallFunction* FEpicUnrealMCPCommonUtils::CreateFunctionCallNode(UEdGraph* Graph, UFunction* Function, const FVector2D& Position)
{
    if (!Graph || !Function)
    {
        return nullptr;
    }
    
    UK2Node_CallFunction* FunctionNode = NewObject<UK2Node_CallFunction>(Graph);
    FunctionNode->SetFromFunction(Function);
    FunctionNode->NodePosX = Position.X;
    FunctionNode->NodePosY = Position.Y;
    Graph->AddNode(FunctionNode, true);
    FunctionNode->CreateNewGuid();
    FunctionNode->PostPlacedNewNode();
    FunctionNode->AllocateDefaultPins();
    
    return FunctionNode;
}

UK2Node_VariableGet* FEpicUnrealMCPCommonUtils::CreateVariableGetNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& VariableName, const FVector2D& Position)
{
    if (!Graph || !Blueprint)
    {
        return nullptr;
    }
    
    UK2Node_VariableGet* VariableGetNode = NewObject<UK2Node_VariableGet>(Graph);
    
    FName VarName(*VariableName);
    FProperty* Property = FindFProperty<FProperty>(Blueprint->GeneratedClass, VarName);
    
    if (Property)
    {
        VariableGetNode->VariableReference.SetFromField<FProperty>(Property, false);
        VariableGetNode->NodePosX = Position.X;
        VariableGetNode->NodePosY = Position.Y;
        Graph->AddNode(VariableGetNode, true);
        VariableGetNode->PostPlacedNewNode();
        VariableGetNode->AllocateDefaultPins();
        
        return VariableGetNode;
    }
    
    return nullptr;
}

UK2Node_VariableSet* FEpicUnrealMCPCommonUtils::CreateVariableSetNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& VariableName, const FVector2D& Position)
{
    if (!Graph || !Blueprint)
    {
        return nullptr;
    }
    
    UK2Node_VariableSet* VariableSetNode = NewObject<UK2Node_VariableSet>(Graph);
    
    FName VarName(*VariableName);
    FProperty* Property = FindFProperty<FProperty>(Blueprint->GeneratedClass, VarName);
    
    if (Property)
    {
        VariableSetNode->VariableReference.SetFromField<FProperty>(Property, false);
        VariableSetNode->NodePosX = Position.X;
        VariableSetNode->NodePosY = Position.Y;
        Graph->AddNode(VariableSetNode, true);
        VariableSetNode->PostPlacedNewNode();
        VariableSetNode->AllocateDefaultPins();
        
        return VariableSetNode;
    }
    
    return nullptr;
}

UK2Node_InputAction* FEpicUnrealMCPCommonUtils::CreateInputActionNode(UEdGraph* Graph, const FString& ActionName, const FVector2D& Position)
{
    if (!Graph)
    {
        return nullptr;
    }
    
    UK2Node_InputAction* InputActionNode = NewObject<UK2Node_InputAction>(Graph);
    InputActionNode->InputActionName = FName(*ActionName);
    InputActionNode->NodePosX = Position.X;
    InputActionNode->NodePosY = Position.Y;
    Graph->AddNode(InputActionNode, true);
    InputActionNode->CreateNewGuid();
    InputActionNode->PostPlacedNewNode();
    InputActionNode->AllocateDefaultPins();
    
    return InputActionNode;
}

UK2Node_Self* FEpicUnrealMCPCommonUtils::CreateSelfReferenceNode(UEdGraph* Graph, const FVector2D& Position)
{
    if (!Graph)
    {
        return nullptr;
    }
    
    UK2Node_Self* SelfNode = NewObject<UK2Node_Self>(Graph);
    SelfNode->NodePosX = Position.X;
    SelfNode->NodePosY = Position.Y;
    Graph->AddNode(SelfNode, true);
    SelfNode->CreateNewGuid();
    SelfNode->PostPlacedNewNode();
    SelfNode->AllocateDefaultPins();
    
    return SelfNode;
}

bool FEpicUnrealMCPCommonUtils::ConnectGraphNodes(UEdGraph* Graph, UEdGraphNode* SourceNode, const FString& SourcePinName, 
                                           UEdGraphNode* TargetNode, const FString& TargetPinName)
{
    if (!Graph || !SourceNode || !TargetNode)
    {
        return false;
    }
    
    UEdGraphPin* SourcePin = FindPin(SourceNode, SourcePinName, EGPD_Output);
    UEdGraphPin* TargetPin = FindPin(TargetNode, TargetPinName, EGPD_Input);
    
    if (SourcePin && TargetPin)
    {
        SourcePin->MakeLinkTo(TargetPin);
        return true;
    }
    
    return false;
}

UEdGraphPin* FEpicUnrealMCPCommonUtils::FindPin(UEdGraphNode* Node, const FString& PinName, EEdGraphPinDirection Direction)
{
    if (!Node)
    {
        return nullptr;
    }
    
    // Log all pins for debugging
    UE_LOG(LogTemp, Display, TEXT("FindPin: Looking for pin '%s' (Direction: %d) in node '%s'"), 
           *PinName, (int32)Direction, *Node->GetName());
    
    for (UEdGraphPin* Pin : Node->Pins)
    {
        UE_LOG(LogTemp, Display, TEXT("  - Available pin: '%s', Direction: %d, Category: %s"), 
               *Pin->PinName.ToString(), (int32)Pin->Direction, *Pin->PinType.PinCategory.ToString());
    }
    
    // First try exact match
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin->PinName.ToString() == PinName && (Direction == EGPD_MAX || Pin->Direction == Direction))
        {
            UE_LOG(LogTemp, Display, TEXT("  - Found exact matching pin: '%s'"), *Pin->PinName.ToString());
            return Pin;
        }
    }
    
    // If no exact match and we're looking for a component reference, try case-insensitive match
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase) && 
            (Direction == EGPD_MAX || Pin->Direction == Direction))
        {
            UE_LOG(LogTemp, Display, TEXT("  - Found case-insensitive matching pin: '%s'"), *Pin->PinName.ToString());
            return Pin;
        }
    }
    
    // If we're looking for a component output and didn't find it by name, try to find the first data output pin
    if (Direction == EGPD_Output && Cast<UK2Node_VariableGet>(Node) != nullptr)
    {
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
            {
                UE_LOG(LogTemp, Display, TEXT("  - Found fallback data output pin: '%s'"), *Pin->PinName.ToString());
                return Pin;
            }
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("  - No matching pin found for '%s'"), *PinName);
    return nullptr;
}

// Actor utilities
TSharedPtr<FJsonValue> FEpicUnrealMCPCommonUtils::ActorToJson(AActor* Actor)
{
    if (!Actor)
    {
        return MakeShared<FJsonValueNull>();
    }
    
    TSharedPtr<FJsonObject> ActorObject = MakeShared<FJsonObject>();
    ActorObject->SetStringField(TEXT("name"), Actor->GetName());
    ActorObject->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
    
    FVector Location = Actor->GetActorLocation();
    TArray<TSharedPtr<FJsonValue>> LocationArray;
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.X));
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Y));
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Z));
    ActorObject->SetArrayField(TEXT("location"), LocationArray);
    
    FRotator Rotation = Actor->GetActorRotation();
    TArray<TSharedPtr<FJsonValue>> RotationArray;
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
    ActorObject->SetArrayField(TEXT("rotation"), RotationArray);
    
    FVector Scale = Actor->GetActorScale3D();
    TArray<TSharedPtr<FJsonValue>> ScaleArray;
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.X));
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Y));
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Z));
    ActorObject->SetArrayField(TEXT("scale"), ScaleArray);
    
    return MakeShared<FJsonValueObject>(ActorObject);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPCommonUtils::ActorToJsonObject(AActor* Actor, bool bDetailed)
{
    if (!Actor)
    {
        return nullptr;
    }
    
    TSharedPtr<FJsonObject> ActorObject = MakeShared<FJsonObject>();
    ActorObject->SetStringField(TEXT("name"), Actor->GetName());
    ActorObject->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
    
    FVector Location = Actor->GetActorLocation();
    TArray<TSharedPtr<FJsonValue>> LocationArray;
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.X));
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Y));
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Z));
    ActorObject->SetArrayField(TEXT("location"), LocationArray);
    
    FRotator Rotation = Actor->GetActorRotation();
    TArray<TSharedPtr<FJsonValue>> RotationArray;
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
    ActorObject->SetArrayField(TEXT("rotation"), RotationArray);
    
    FVector Scale = Actor->GetActorScale3D();
    TArray<TSharedPtr<FJsonValue>> ScaleArray;
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.X));
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Y));
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Z));
    ActorObject->SetArrayField(TEXT("scale"), ScaleArray);
    
    return ActorObject;
}

UK2Node_Event* FEpicUnrealMCPCommonUtils::FindExistingEventNode(UEdGraph* Graph, const FString& EventName)
{
    if (!Graph)
    {
        return nullptr;
    }

    // Look for existing event nodes
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
        if (EventNode && EventNode->EventReference.GetMemberName() == FName(*EventName))
        {
            UE_LOG(LogTemp, Display, TEXT("Found existing event node with name: %s"), *EventName);
            return EventNode;
        }
    }

    return nullptr;
}

bool FEpicUnrealMCPCommonUtils::SetObjectProperty(UObject* Object, const FString& PropertyName, 
                                     const TSharedPtr<FJsonValue>& Value, FString& OutErrorMessage)
{
    if (!Object)
    {
        OutErrorMessage = TEXT("Invalid object");
        return false;
    }

    FProperty* Property = Object->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property)
    {
        OutErrorMessage = FString::Printf(TEXT("Property not found: %s"), *PropertyName);
        return false;
    }

    void* PropertyAddr = Property->ContainerPtrToValuePtr<void>(Object);
    
    // Handle different property types
    if (Property->IsA<FBoolProperty>())
    {
        ((FBoolProperty*)Property)->SetPropertyValue(PropertyAddr, Value->AsBool());
        return true;
    }
    else if (Property->IsA<FIntProperty>())
    {
        int32 IntValue = static_cast<int32>(Value->AsNumber());
        FIntProperty* IntProperty = CastField<FIntProperty>(Property);
        if (IntProperty)
        {
            IntProperty->SetPropertyValue_InContainer(Object, IntValue);
            return true;
        }
    }
    else if (Property->IsA<FFloatProperty>())
    {
        ((FFloatProperty*)Property)->SetPropertyValue(PropertyAddr, Value->AsNumber());
        return true;
    }
    else if (Property->IsA<FStrProperty>())
    {
        ((FStrProperty*)Property)->SetPropertyValue(PropertyAddr, Value->AsString());
        return true;
    }
    else if (Property->IsA<FByteProperty>())
    {
        FByteProperty* ByteProp = CastField<FByteProperty>(Property);
        UEnum* EnumDef = ByteProp ? ByteProp->GetIntPropertyEnum() : nullptr;
        
        // If this is a TEnumAsByte property (has associated enum)
        if (EnumDef)
        {
            // Handle numeric value
            if (Value->Type == EJson::Number)
            {
                uint8 ByteValue = static_cast<uint8>(Value->AsNumber());
                ByteProp->SetPropertyValue(PropertyAddr, ByteValue);
                
                UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to numeric value: %d"), 
                      *PropertyName, ByteValue);
                return true;
            }
            // Handle string enum value
            else if (Value->Type == EJson::String)
            {
                FString EnumValueName = Value->AsString();
                
                // Try to convert numeric string to number first
                if (EnumValueName.IsNumeric())
                {
                    uint8 ByteValue = FCString::Atoi(*EnumValueName);
                    ByteProp->SetPropertyValue(PropertyAddr, ByteValue);
                    
                    UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to numeric string value: %s -> %d"), 
                          *PropertyName, *EnumValueName, ByteValue);
                    return true;
                }
                
                // Handle qualified enum names (e.g., "Player0" or "EAutoReceiveInput::Player0")
                if (EnumValueName.Contains(TEXT("::")))
                {
                    EnumValueName.Split(TEXT("::"), nullptr, &EnumValueName);
                }
                
                int64 EnumValue = EnumDef->GetValueByNameString(EnumValueName);
                if (EnumValue == INDEX_NONE)
                {
                    // Try with full name as fallback
                    EnumValue = EnumDef->GetValueByNameString(Value->AsString());
                }
                
                if (EnumValue != INDEX_NONE)
                {
                    ByteProp->SetPropertyValue(PropertyAddr, static_cast<uint8>(EnumValue));
                    
                    UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to name value: %s -> %lld"), 
                          *PropertyName, *EnumValueName, EnumValue);
                    return true;
                }
                else
                {
                    // Log all possible enum values for debugging
                    UE_LOG(LogTemp, Warning, TEXT("Could not find enum value for '%s'. Available options:"), *EnumValueName);
                    for (int32 i = 0; i < EnumDef->NumEnums(); i++)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("  - %s (value: %d)"), 
                               *EnumDef->GetNameStringByIndex(i), EnumDef->GetValueByIndex(i));
                    }
                    
                    OutErrorMessage = FString::Printf(TEXT("Could not find enum value for '%s'"), *EnumValueName);
                    return false;
                }
            }
        }
        else
        {
            // Regular byte property
            uint8 ByteValue = static_cast<uint8>(Value->AsNumber());
            ByteProp->SetPropertyValue(PropertyAddr, ByteValue);
            return true;
        }
    }
    else if (Property->IsA<FEnumProperty>())
    {
        FEnumProperty* EnumProp = CastField<FEnumProperty>(Property);
        UEnum* EnumDef = EnumProp ? EnumProp->GetEnum() : nullptr;
        FNumericProperty* UnderlyingNumericProp = EnumProp ? EnumProp->GetUnderlyingProperty() : nullptr;
        
        if (EnumDef && UnderlyingNumericProp)
        {
            // Handle numeric value
            if (Value->Type == EJson::Number)
            {
                int64 EnumValue = static_cast<int64>(Value->AsNumber());
                UnderlyingNumericProp->SetIntPropertyValue(PropertyAddr, EnumValue);
                
                UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to numeric value: %lld"), 
                      *PropertyName, EnumValue);
                return true;
            }
            // Handle string enum value
            else if (Value->Type == EJson::String)
            {
                FString EnumValueName = Value->AsString();
                
                // Try to convert numeric string to number first
                if (EnumValueName.IsNumeric())
                {
                    int64 EnumValue = FCString::Atoi64(*EnumValueName);
                    UnderlyingNumericProp->SetIntPropertyValue(PropertyAddr, EnumValue);
                    
                    UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to numeric string value: %s -> %lld"), 
                          *PropertyName, *EnumValueName, EnumValue);
                    return true;
                }
                
                // Handle qualified enum names
                if (EnumValueName.Contains(TEXT("::")))
                {
                    EnumValueName.Split(TEXT("::"), nullptr, &EnumValueName);
                }
                
                int64 EnumValue = EnumDef->GetValueByNameString(EnumValueName);
                if (EnumValue == INDEX_NONE)
                {
                    // Try with full name as fallback
                    EnumValue = EnumDef->GetValueByNameString(Value->AsString());
                }
                
                if (EnumValue != INDEX_NONE)
                {
                    UnderlyingNumericProp->SetIntPropertyValue(PropertyAddr, EnumValue);
                    
                    UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to name value: %s -> %lld"), 
                          *PropertyName, *EnumValueName, EnumValue);
                    return true;
                }
                else
                {
                    // Log all possible enum values for debugging
                    UE_LOG(LogTemp, Warning, TEXT("Could not find enum value for '%s'. Available options:"), *EnumValueName);
                    for (int32 i = 0; i < EnumDef->NumEnums(); i++)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("  - %s (value: %d)"), 
                               *EnumDef->GetNameStringByIndex(i), EnumDef->GetValueByIndex(i));
                    }
                    
                    OutErrorMessage = FString::Printf(TEXT("Could not find enum value for '%s'"), *EnumValueName);
                    return false;
                }
            }
        }
    }
    
    OutErrorMessage = FString::Printf(TEXT("Unsupported property type: %s for property %s"), 
                                    *Property->GetClass()->GetName(), *PropertyName);
    return false;
} 

TSharedPtr<FJsonValue> FEpicUnrealMCPCommonUtils::GetObjectPropertyAsJson(UObject* Object, const FString& PropertyName,
    int32 MaxDepth, bool bIncludeAllProperties)
{
    if (!Object)
    {
        return MakeShared<FJsonValueNull>();
    }

    FProperty* Property = Object->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property)
    {
        for (TFieldIterator<FProperty> PropIt(Object->GetClass(), EFieldIterationFlags::IncludeSuper); PropIt; ++PropIt)
        {
            if (PropIt->GetName().Equals(PropertyName, ESearchCase::IgnoreCase))
            {
                Property = *PropIt;
                break;
            }
        }
    }

    if (!Property)
    {
        return MakeShared<FJsonValueNull>();
    }

    const void* PropertyAddr = Property->ContainerPtrToValuePtr<const void>(Object);
    TSet<const UObject*> VisitedObjects;
    return SerializePropertyValue(Property, PropertyAddr, FMath::Clamp(MaxDepth, 0, 8), bIncludeAllProperties, VisitedObjects);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPCommonUtils::GetAllObjectPropertiesAsJson(UObject* Object,
    int32 MaxDepth, bool bIncludeAllProperties)
{
    TSet<const UObject*> VisitedObjects;
    if (Object)
    {
        VisitedObjects.Add(Object);
    }
    return SerializeStructProperties(Object ? Object->GetClass() : nullptr, Object, FMath::Clamp(MaxDepth, 0, 8), bIncludeAllProperties, VisitedObjects);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPCommonUtils::GetStructPropertiesAsJson(const UStruct* StructType, const void* StructData,
    int32 MaxDepth, bool bIncludeAllProperties)
{
    TSet<const UObject*> VisitedObjects;
    return SerializeStructProperties(StructType, StructData, FMath::Clamp(MaxDepth, 0, 8), bIncludeAllProperties, VisitedObjects);
}

TSharedPtr<FJsonObject> FEpicUnrealMCPCommonUtils::GetObjectReferenceAsJson(UObject* Object,
    int32 MaxDepth, bool bIncludeAllProperties)
{
    TSharedPtr<FJsonObject> ObjectJson = BuildObjectSummary(Object);
    if (!Object)
    {
        return ObjectJson;
    }

    if (MaxDepth > 0)
    {
        TSet<const UObject*> VisitedObjects;
        VisitedObjects.Add(Object);
        ObjectJson->SetObjectField(TEXT("properties"), SerializeStructProperties(Object->GetClass(), Object, FMath::Clamp(MaxDepth - 1, 0, 8), bIncludeAllProperties, VisitedObjects));
    }

    return ObjectJson;
}