#include "Commands/BlueprintGraph/BPConnector.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "Engine/Blueprint.h"
#include "K2Node.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EditorAssetLibrary.h"

TSharedPtr<FJsonObject> FBPConnector::ConnectNodes(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

    // Extraire paramètres
    FString BlueprintName = Params->GetStringField(TEXT("blueprint_name"));
    FString SourceNodeId = Params->GetStringField(TEXT("source_node_id"));
    FString SourcePinName = Params->GetStringField(TEXT("source_pin_name"));
    FString TargetNodeId = Params->GetStringField(TEXT("target_node_id"));
    FString TargetPinName = Params->GetStringField(TEXT("target_pin_name"));

    FString FunctionName;
    Params->TryGetStringField(TEXT("function_name"), FunctionName);

    // Charger Blueprint - handle both full paths and simple names
    UBlueprint* Blueprint = nullptr;
    FString BlueprintPath = BlueprintName;

    // If no path prefix, assume /Game/Blueprints/
    if (!BlueprintPath.StartsWith(TEXT("/")))
    {
        BlueprintPath = TEXT("/Game/Blueprints/") + BlueprintPath;
    }

    // Add .Blueprint suffix if not present
    if (!BlueprintPath.Contains(TEXT(".")))
    {
        BlueprintPath += TEXT(".") + FPaths::GetBaseFilename(BlueprintPath);
    }

    // Try to load the Blueprint
    Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);

    // If not found, try with UEditorAssetLibrary
    if (!Blueprint)
    {
        FString AssetPath = BlueprintPath;
        if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
        {
            UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
            Blueprint = Cast<UBlueprint>(Asset);
        }
    }

    if (!Blueprint)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Blueprint not found");
        return Result;
    }

    // Get graph
    UEdGraph* Graph = nullptr;

    if (!FunctionName.IsEmpty())
    {
        // Strategy 1: Try exact name match with GetFName()
        for (UEdGraph* FuncGraph : Blueprint->FunctionGraphs)
        {
            if (FuncGraph && (FuncGraph->GetFName().ToString() == FunctionName ||
                              (FuncGraph->GetOuter() && FuncGraph->GetOuter()->GetFName().ToString() == FunctionName)))
            {
                Graph = FuncGraph;
                break;
            }
        }

        // Strategy 2: Fallback - partial match for auto-generated names
        if (!Graph)
        {
            for (UEdGraph* FuncGraph : Blueprint->FunctionGraphs)
            {
                if (FuncGraph && FuncGraph->GetFName().ToString().Contains(FunctionName))
                {
                    Graph = FuncGraph;
                    break;
                }
            }
        }

        if (!Graph)
        {
            Result->SetBoolField("success", false);
            Result->SetStringField("error", FString::Printf(TEXT("Function graph not found: %s"), *FunctionName));
            return Result;
        }
    }
    else
    {
        // Use event graph if no function specified
        if (Blueprint->UbergraphPages.Num() == 0)
        {
            Result->SetBoolField("success", false);
            Result->SetStringField("error", "Blueprint has no event graph");
            return Result;
        }

        Graph = Blueprint->UbergraphPages[0];
    }

    if (!Graph)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Graph not found");
        return Result;
    }

    // Find nodes
    UK2Node* SourceNode = FindNodeById(Graph, SourceNodeId);
    UK2Node* TargetNode = FindNodeById(Graph, TargetNodeId);

    if (!SourceNode || !TargetNode)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Node not found");
        return Result;
    }

    // Trouver pins
    UEdGraphPin* SourcePin = FindPinByName(SourceNode, SourcePinName, EGPD_Output);
    UEdGraphPin* TargetPin = FindPinByName(TargetNode, TargetPinName, EGPD_Input);

    if (!SourcePin || !TargetPin)
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Pin not found");
        return Result;
    }

    // Validate compatibility
    if (!ArePinsCompatible(SourcePin, TargetPin))
    {
        Result->SetBoolField("success", false);
        Result->SetStringField("error", "Pins not compatible");
        return Result;
    }

    // Create connection
    SourcePin->MakeLinkTo(TargetPin);

    // Recompile
    Blueprint->MarkPackageDirty();
    FKismetEditorUtilities::CompileBlueprint(Blueprint);

    // Return
    Result->SetBoolField("success", true);

    TSharedPtr<FJsonObject> ConnectionInfo = MakeShared<FJsonObject>();
    ConnectionInfo->SetStringField("source_node", SourceNodeId);
    ConnectionInfo->SetStringField("source_pin", SourcePinName);
    ConnectionInfo->SetStringField("target_node", TargetNodeId);
    ConnectionInfo->SetStringField("target_pin", TargetPinName);
    ConnectionInfo->SetStringField("connection_type", SourcePin->PinType.PinCategory.ToString());

    Result->SetObjectField("connection", ConnectionInfo);

    return Result;
}

UK2Node* FBPConnector::FindNodeById(UEdGraph* Graph, const FString& NodeId)
{
    if (!Graph)
    {
        return nullptr;
    }

    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (!Node)
        {
            continue;
        }

        // Try matching by NodeGuid first
        if (Node->NodeGuid.ToString().Equals(NodeId, ESearchCase::IgnoreCase))
        {
            UK2Node* K2Node = Cast<UK2Node>(Node);
            return K2Node;  // Return even if nullptr (caller will handle)
        }

        // Try matching by GetName()
        if (Node->GetName().Equals(NodeId, ESearchCase::IgnoreCase))
        {
            UK2Node* K2Node = Cast<UK2Node>(Node);
            return K2Node;  // Return even if nullptr (caller will handle)
        }
    }

    return nullptr;
}

UEdGraphPin* FBPConnector::FindPinByName(UK2Node* Node, const FString& PinName, EEdGraphPinDirection Direction)
{
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin->PinName.ToString() == PinName && Pin->Direction == Direction)
        {
            return Pin;
        }
    }
    return nullptr;
}

bool FBPConnector::ArePinsCompatible(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin)
{
    if (SourcePin->Direction != EGPD_Output || TargetPin->Direction != EGPD_Input)
    {
        return false;
    }

    return SourcePin->PinType.PinCategory == TargetPin->PinType.PinCategory;
}

TSharedPtr<FJsonObject> FBPConnector::DisconnectNodes(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

    FString BlueprintName;
    FString SourceNodeId;
    FString SourcePinName;
    FString TargetNodeId;
    FString TargetPinName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName) ||
        !Params->TryGetStringField(TEXT("source_node_id"), SourceNodeId) ||
        !Params->TryGetStringField(TEXT("source_pin_name"), SourcePinName) ||
        !Params->TryGetStringField(TEXT("target_node_id"), TargetNodeId) ||
        !Params->TryGetStringField(TEXT("target_pin_name"), TargetPinName))
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Missing Blueprint connection parameter"));
        return Result;
    }

    FString BlueprintPath = BlueprintName;
    if (!BlueprintPath.StartsWith(TEXT("/")))
    {
        BlueprintPath = TEXT("/Game/Blueprints/") + BlueprintPath;
    }
    if (!BlueprintPath.Contains(TEXT(".")))
    {
        BlueprintPath += TEXT(".") + FPaths::GetBaseFilename(BlueprintPath);
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!Blueprint && UEditorAssetLibrary::DoesAssetExist(BlueprintPath))
    {
        Blueprint = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
    }
    if (!Blueprint)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Blueprint not found"));
        return Result;
    }

    FString FunctionName;
    Params->TryGetStringField(TEXT("function_name"), FunctionName);
    UEdGraph* Graph = nullptr;
    if (FunctionName.IsEmpty())
    {
        Graph = Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
    }
    else
    {
        for (UEdGraph* FunctionGraph : Blueprint->FunctionGraphs)
        {
            if (FunctionGraph && FunctionGraph->GetFName().ToString().Equals(FunctionName, ESearchCase::IgnoreCase))
            {
                Graph = FunctionGraph;
                break;
            }
        }
    }
    if (!Graph)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Blueprint graph not found"));
        return Result;
    }

    UK2Node* SourceNode = FindNodeById(Graph, SourceNodeId);
    UK2Node* TargetNode = FindNodeById(Graph, TargetNodeId);
    UEdGraphPin* SourcePin = SourceNode ? FindPinByName(SourceNode, SourcePinName, EGPD_Output) : nullptr;
    UEdGraphPin* TargetPin = TargetNode ? FindPinByName(TargetNode, TargetPinName, EGPD_Input) : nullptr;
    if (!SourcePin || !TargetPin)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Node or pin not found"));
        return Result;
    }
    if (!SourcePin->LinkedTo.Contains(TargetPin))
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error"), TEXT("Pins are not connected"));
        return Result;
    }

    SourcePin->BreakLinkTo(TargetPin);
    Graph->NotifyGraphChanged();
    Blueprint->MarkPackageDirty();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);

    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("source_node"), SourceNodeId);
    Result->SetStringField(TEXT("source_pin"), SourcePinName);
    Result->SetStringField(TEXT("target_node"), TargetNodeId);
    Result->SetStringField(TEXT("target_pin"), TargetPinName);
    return Result;
}
