#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Containers/Ticker.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "AI/Navigation/NavQueryFilter.h"
#include "Detour/DetourNavMesh.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMisc.h"

DEFINE_LOG_CATEGORY_STATIC(LogBtNavExport, Log, All);

namespace
{
template <typename T>
TArray<TSharedPtr<FJsonValue>> Numbers(const T* Values, int32 Count)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    Result.Reserve(Count);
    for (int32 I = 0; I < Count; ++I)
        Result.Add(MakeShared<FJsonValueNumber>(static_cast<double>(Values[I])));
    return Result;
}

TArray<TSharedPtr<FJsonValue>> Point(const FVector& P)
{
    const double Values[] = {P.X, P.Y, P.Z};
    return Numbers(Values, 3);
}

bool Export(UWorld& World, ARecastNavMesh& Nav, const FString& Output, FString& Error)
{
    const dtNavMesh* Mesh = Nav.GetRecastMesh();
    if (!Mesh) { Error = TEXT("No saved Recast mesh; build navigation and save Forest first."); return false; }

    if (World.GetWorldPartition()) { Error = TEXT("World Partition requires a full-world export workflow; refusing a possibly partial mesh."); return false; }

    const auto* Params = Mesh->getParams();
    const auto Filter = Nav.GetDefaultQueryFilter();
    float Costs[DT_MAX_AREAS], FixedCosts[DT_MAX_AREAS];
    Filter->GetAllAreaCosts(Costs, FixedCosts, DT_MAX_AREAS);
    const auto Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("format"), TEXT("bt-navmesh"));
    Root->SetNumberField(TEXT("version"), 1);
    Root->SetStringField(TEXT("map"), World.GetOutermost()->GetName());
    Root->SetStringField(TEXT("coordinates"), TEXT("recast_cm"));
    Root->SetStringField(TEXT("sourceEngine"), FEngineVersion::Current().ToString());
    Root->SetArrayField(TEXT("origin"), Numbers(Params->orig, 3));
    Root->SetNumberField(TEXT("tileWidth"), Params->tileWidth);
    Root->SetNumberField(TEXT("tileHeight"), Params->tileHeight);
    Root->SetNumberField(TEXT("agentRadius"), Params->walkableRadius);
    Root->SetNumberField(TEXT("agentHeight"), Params->walkableHeight);
    Root->SetNumberField(TEXT("includeFlags"), Filter->GetIncludeFlags());
    Root->SetNumberField(TEXT("excludeFlags"), Filter->GetExcludeFlags());
    Root->SetArrayField(TEXT("areaCosts"), Numbers(Costs, DT_MAX_AREAS));
    TArray<TSharedPtr<FJsonValue>> Tiles;
    int32 PolygonCount = 0;
    for (int32 Index = 0; Index < Mesh->getMaxTiles(); ++Index)
    {
        const dtMeshTile* Tile = Mesh->getTile(Index);
        if (!Tile || !Tile->header || !Tile->header->polyCount) continue;
        const auto& H = *Tile->header;
        if (H.offMeshConCount || H.detailMeshCount != H.polyCount)
        { Error = TEXT("Off-mesh links or missing detail meshes are unsupported."); return false; }

        const auto Resolution = static_cast<ENavigationDataResolution>(H.resolution);
        const double Cs = Nav.GetCellSize(Resolution), Ch = Nav.GetCellHeight(Resolution);
        if (Cs <= 0 || Ch <= 0) { Error = TEXT("Invalid cell dimensions."); return false; }

        TArray<uint16> Vertices, Polygons, Flags;
        TArray<uint8> Areas, Triangles;
        TArray<uint32> Details;
        TArray<double> DetailVertices;
        for (int32 V = 0; V < H.vertCount; ++V)
        {
            for (int32 Axis = 0; Axis < 3; ++Axis)
            {
                const double Cell = Axis == 1 ? Ch : Cs;
                const double Q = (Tile->verts[V * 3 + Axis] - H.bmin[Axis]) / Cell;
                const double Rounded = FMath::RoundToDouble(Q);
                if (!FMath::IsFinite(Q) || Rounded < 0 || Rounded > 65535 || FMath::Abs(Q - Rounded) > 0.01)
                { Error = TEXT("Vertex is not on the expected UE 5.7 tile grid."); return false; }

                Vertices.Add(static_cast<uint16>(Rounded));
            }
        }

        for (int32 P = 0; P < H.polyCount; ++P)
        {
            const auto& Poly = Tile->polys[P];
            if (Poly.getType() != DT_POLYTYPE_GROUND || Poly.vertCount < 3 || Poly.vertCount > 6 ||
                FixedCosts[Poly.getArea()] != 0 || Costs[Poly.getArea()] < 1)
            { Error = TEXT("Unsupported polygon type, fixed entry cost or area cost below one."); return false; }

            for (int32 E = 0; E < 6; ++E) Polygons.Add(E < Poly.vertCount ? Poly.verts[E] : 65535);
            for (int32 E = 0; E < 6; ++E)
            {
                uint16 Neighbor = 65535;
                if (E < Poly.vertCount && Poly.neis[E])
                {
                    const uint16 N = Poly.neis[E];
                    if (N & DT_EXT_LINK)
                    {
                        switch (N & 0xff)
                        {
                        case 4: Neighbor = 0x8000; break;
                        case 2: Neighbor = 0x8001; break;
                        case 0: Neighbor = 0x8002; break;
                        case 6: Neighbor = 0x8003; break;
                        default: Error = TEXT("Unsupported external portal direction."); return false;
                        }
                    }
                    else Neighbor = N - 1;
                }

                Polygons.Add(Neighbor);
            }

            Flags.Add(Poly.flags);
            Areas.Add(Poly.getArea());
            const auto& D = Tile->detailMeshes[P];
            Details.Add(DetailVertices.Num() / 3);
            Details.Add(Poly.vertCount + D.vertCount);
            Details.Add(Triangles.Num() / 4);
            Details.Add(D.triCount);
            for (int32 V = 0; V < Poly.vertCount; ++V)
                for (int32 A = 0; A < 3; ++A) DetailVertices.Add(Tile->verts[Poly.verts[V] * 3 + A]);
            for (int32 V = 0; V < D.vertCount; ++V)
                for (int32 A = 0; A < 3; ++A) DetailVertices.Add(Tile->detailVerts[(D.vertBase + V) * 3 + A]);
            for (int32 T = 0; T < D.triCount * 4; ++T) Triangles.Add(Tile->detailTris[D.triBase * 4 + T]);
        }

        const auto JsonTile = MakeShared<FJsonObject>();
        JsonTile->SetNumberField(TEXT("x"), H.x);
        JsonTile->SetNumberField(TEXT("y"), H.y);
        JsonTile->SetNumberField(TEXT("layer"), H.layer);
        JsonTile->SetNumberField(TEXT("cellSize"), Cs);
        JsonTile->SetNumberField(TEXT("cellHeight"), Ch);
        JsonTile->SetNumberField(TEXT("walkableClimb"), Nav.GetAgentMaxStepHeight(Resolution));
        JsonTile->SetArrayField(TEXT("bmin"), Numbers(H.bmin, 3));
        JsonTile->SetArrayField(TEXT("bmax"), Numbers(H.bmax, 3));
        JsonTile->SetArrayField(TEXT("vertices"), Numbers(Vertices.GetData(), Vertices.Num()));
        JsonTile->SetArrayField(TEXT("polygons"), Numbers(Polygons.GetData(), Polygons.Num()));
        JsonTile->SetArrayField(TEXT("flags"), Numbers(Flags.GetData(), Flags.Num()));
        JsonTile->SetArrayField(TEXT("areas"), Numbers(Areas.GetData(), Areas.Num()));
        JsonTile->SetArrayField(TEXT("detailMeshes"), Numbers(Details.GetData(), Details.Num()));
        JsonTile->SetArrayField(TEXT("detailVertices"), Numbers(DetailVertices.GetData(), DetailVertices.Num()));
        JsonTile->SetArrayField(TEXT("detailTriangles"), Numbers(Triangles.GetData(), Triangles.Num()));
        Tiles.Add(MakeShared<FJsonValueObject>(JsonTile));
        PolygonCount += H.polyCount;
    }

    if (Tiles.IsEmpty()) { Error = TEXT("No navigation tiles in saved map."); return false; }

    Root->SetNumberField(TEXT("polygonCount"), PolygonCount);
    Root->SetArrayField(TEXT("tiles"), Tiles);

    TArray<TSharedPtr<FJsonValue>> ReferencePaths;
    auto* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(&World);
    FNavLocation Start;
    for (TActorIterator<APlayerStart> It(&World); It; ++It)
    {
        if (NavSystem->ProjectPointToNavigation(It->GetActorLocation(), Start, FVector(100, 100, 500), &Nav)) break;
    }

    if (Start.NodeRef)
    {
        const FBox Bounds = Nav.GetNavMeshBounds();
        for (int32 X = 1; X <= 5; ++X)
        for (int32 Y = 1; Y <= 5; ++Y)
        {
            const FVector Candidate(FMath::Lerp(Bounds.Min.X, Bounds.Max.X, X / 6.0),
                FMath::Lerp(Bounds.Min.Y, Bounds.Max.Y, Y / 6.0), Bounds.GetCenter().Z);
            FNavLocation End;
            if (!NavSystem->ProjectPointToNavigation(Candidate, End, FVector(200, 200, Bounds.GetExtent().Z + 100), &Nav)) continue;
            FPathFindingQuery Query(nullptr, Nav, Start.Location, End.Location, Filter);
            const auto Path = NavSystem->FindPathSync(Query);
            if (!Path.IsSuccessful() || Path.Path->IsPartial()) continue;
            const auto Entry = MakeShared<FJsonObject>();
            Entry->SetArrayField(TEXT("start"), Point(Start.Location));
            Entry->SetArrayField(TEXT("end"), Point(End.Location));
            ReferencePaths.Add(MakeShared<FJsonValueObject>(Entry));
        }
    }

    Root->SetArrayField(TEXT("referencePaths"), ReferencePaths);
    FString Text;
    const auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text);
    if (!FJsonSerializer::Serialize(Root, Writer) || !FFileHelper::SaveStringToFile(Text, *Output, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    { Error = TEXT("Unable to write navigation JSON."); return false; }

    UE_LOG(LogBtNavExport, Display, TEXT("EXPORTED map=%s tiles=%d polygons=%d referencePaths=%d"),
        *World.GetOutermost()->GetName(), Tiles.Num(), PolygonCount, ReferencePaths.Num());
    return true;
}
}

class FBtNavMeshExportModule final : public IModuleInterface
{
    FTSTicker::FDelegateHandle Ticker;
    FString Output;
    double Started = 0;
public:
    virtual void StartupModule() override
    {
        if (!FParse::Value(FCommandLine::Get(), TEXT("BtNavExport="), Output)) return;
        Started = FPlatformTime::Seconds();
        Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FBtNavMeshExportModule::Tick), 1.0f);
    }

    virtual void ShutdownModule() override { FTSTicker::GetCoreTicker().RemoveTicker(Ticker); }

    bool Tick(float)
    {
        if (FPlatformTime::Seconds() - Started > 180)
        {
            UE_LOG(LogBtNavExport, Error, TEXT("Timed out waiting for saved map navigation."));
            FPlatformMisc::RequestExitWithStatus(false, 1);
            return false;
        }

        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        auto* System = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
        auto* Nav = System ? Cast<ARecastNavMesh>(System->GetDefaultNavDataInstance(FNavigationSystem::DontCreate)) : nullptr;
        if (!Nav || System->IsNavigationBuildInProgress() || FPlatformTime::Seconds() - Started < 5) return true;
        FString Error;
        const bool Success = Export(*World, *Nav, Output, Error);
        if (!Success) UE_LOG(LogBtNavExport, Error, TEXT("%s"), *Error);
        FPlatformMisc::RequestExitWithStatus(false, Success ? 0 : 1);
        return false;
    }
};

IMPLEMENT_MODULE(FBtNavMeshExportModule, BtNavMeshExport)
