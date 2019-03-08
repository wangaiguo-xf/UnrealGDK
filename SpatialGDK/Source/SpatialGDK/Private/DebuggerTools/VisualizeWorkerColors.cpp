#include "VisualizeWorkerColors.h"
#include "BoundaryCube.h"
#include "UnrealNetwork.h"
#include "SpatialGDkSettings.h"

#pragma  optimize("", off)

AVisualizeWorkerColors::AVisualizeWorkerColors()
{
	PrimaryActorTick.bCanEverTick = true;
	LastSpawnedIndex	 = 0;
	bReplicates          = true;
	ObjectColorsInWorker = FColor::MakeRandomColor();
}

void AVisualizeWorkerColors::BeginPlay()
{
	Super::BeginPlay();
}

void AVisualizeWorkerColors::OnAuthorityGained()
{
	const USpatialGDKSettings* const GDKSettings = GetDefault<USpatialGDKSettings>();
	if (GDKSettings->ConstructWorkerBoundaries)
	{
		InitGrid2D();
		FTimerHandle UnusedHandle;
		GetWorldTimerManager().SetTimer(UnusedHandle, this, &AVisualizeWorkerColors::SpawnBoundaryCubes, GDKSettings->DelayToStartSpawningCubes, false);
	}
}

void AVisualizeWorkerColors::OnAuthorityLost()
{

}

void AVisualizeWorkerColors::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

bool AVisualizeWorkerColors::BeginCreatingWalls_Validate()
{
	return true;
}
void AVisualizeWorkerColors::BeginCreatingWalls_Implementation()
{
	for (auto& EntryIn : Grid2D)
	{
		if (!EntryIn.DebugCube.IsValid() || EntryIn.DebugCube->GetCurrentMeshColor() == FColor::Black )
		{
			FTimerHandle UnusedHandle;
			GetWorldTimerManager().SetTimer(UnusedHandle, this, &AVisualizeWorkerColors::BeginCreatingWalls, 2.0f, false);
			return;
		}
	}

	UpdateCubeVisibility();
	DeleteBoundaryWalls();
	CreateBoundaryWalls();
	TurnOffAllCubeVisibility();
}

void AVisualizeWorkerColors::UpdateCubeVisibility()
{
	const USpatialGDKSettings* const GDKSettings = GetDefault<USpatialGDKSettings>();
	for (int i = 0; i < Grid2D.Num(); ++i)
	{
		const uint32 Width = GDKSettings->WorldDimensionX / GDKSettings->ChunkEdgeLength;

		const uint32 leftUpperIndex  = i - Width - 1;
		const uint32 upperIndex      = i - Width;
		const uint32 rightUpperIndex = i - Width + 1;

		const uint32 leftIndex       = i - 1;
		const uint32 rightIndex      = i + 1;

		const uint32 leftLowerIndex  = i + Width - 1;
		const uint32 lowerIndex      = i + Width;
		const uint32 rightLowerIndex = i + Width + 1;

		TArray<uint32> CompareTo{ leftUpperIndex, upperIndex, rightUpperIndex, leftIndex, rightIndex, leftLowerIndex, lowerIndex, rightLowerIndex };

		CompareChuncks(i, CompareTo);
	}
}

void AVisualizeWorkerColors::DeleteBoundaryWalls()
{
	for(auto& EntryIn : CachedBoundaryWalls)
	{
		EntryIn->DestroyCube();
	}
	CachedBoundaryWalls.Empty();
}

void AVisualizeWorkerColors::CreateBoundaryWalls()
{
	const USpatialGDKSettings* const GDKSettings = GetDefault<USpatialGDKSettings>();

	const int Width = GDKSettings->WorldDimensionX / GDKSettings->ChunkEdgeLength;
	const int Height = GDKSettings->WorldDimensionZ / GDKSettings->ChunkEdgeLength;

	const FRotator DefaultRotator{ 0, 0, 0 };
	const float Offset = GDKSettings->ChunkEdgeLength / 2.0f * 100.0f - 50.0f;

	TArray<FTransform> WallSpawnData;

	for (int EndIndex = 0; EndIndex < Grid2D.Num(); ++EndIndex)
	{
		// Check if we are at the end of a row.
		// If so, we know we aren't going to have a wall so move on. 
		if ((EndIndex + 1) % Width == 0)
		{
			continue;
		}

		const int StartIndex = EndIndex;

		const FDebugBoundaryInfo InfoStart = Grid2D[StartIndex];

		
		while (EndIndex < Grid2D.Num() && Grid2D[EndIndex].bIsVisible && AreColorsMatching(StartIndex, EndIndex))
		{
			++EndIndex;
		}

		
		if (StartIndex < EndIndex - 1)
		{
			--EndIndex;
			const FDebugBoundaryInfo InfoEnd = Grid2D[EndIndex];
			FVector Position = (InfoStart.SpawnPosition + InfoEnd.SpawnPosition) / 2.0f;

			if (StartIndex + Width < Grid2D.Num() && AreColorsMatching(StartIndex, StartIndex + Width))
			{
				Position.X -= Offset;
			}
			else
			{
				Position.X += Offset;
			}
			WallSpawnData.Add(FTransform(DefaultRotator, Position, FVector(1, FVector::Dist2D(InfoStart.SpawnPosition, InfoEnd.SpawnPosition) / 100.0f + GDKSettings->ChunkEdgeLength, GDKSettings->WallScaleZ)));
		}
	}

	
	TSet<int> IndexesUsed;

	for (int StartIndex = 0; StartIndex < Grid2D.Num(); ++StartIndex)
	{
		int EndIndex = StartIndex;

		const FDebugBoundaryInfo InfoStart = Grid2D[StartIndex];

		while (EndIndex < Grid2D.Num() && Grid2D[EndIndex].bIsVisible && AreColorsMatching(StartIndex, EndIndex) && !IndexesUsed.Find(EndIndex))
		{
			IndexesUsed.Add(EndIndex);
			EndIndex += Width;
		}

		EndIndex -= Width;
		if (StartIndex < EndIndex)
		{
			const FDebugBoundaryInfo InfoEnd = Grid2D[EndIndex];

			FVector Position = (InfoStart.SpawnPosition + InfoEnd.SpawnPosition) / 2.0f;

			if (StartIndex + 1 < Grid2D.Num() && AreColorsMatching(StartIndex, StartIndex + 1))
			{
				Position.Y -= Offset;
			}
			else
			{
				Position.Y += Offset;
			}

			WallSpawnData.Add(FTransform(DefaultRotator, Position, FVector(FVector::Dist2D(InfoStart.SpawnPosition, InfoEnd.SpawnPosition) / 100.0f + GDKSettings->ChunkEdgeLength, 1, GDKSettings->WallScaleZ)));
		}
	}

 	for (auto& EntryIn : WallSpawnData)
	{
		ABoundaryCube* DebugCube = GetWorld()->SpawnActor<ABoundaryCube>(ABoundaryCube::StaticClass(), EntryIn);
		DebugCube->Server_SetVisibility(true);
		CachedBoundaryWalls.Add(DebugCube);
	}
}

bool AVisualizeWorkerColors::AreColorsMatching(const uint32 InLhs, const uint32 InRhs)
{
	return Grid2D[InLhs].DebugCube->GetCurrentMeshColor() == Grid2D[InRhs].DebugCube->GetCurrentMeshColor();
}

void AVisualizeWorkerColors::TurnOffAllCubeVisibility()
{
	for (auto& EntryIn : Grid2D)
	{
		if (EntryIn.bIsVisible)
		{
			EntryIn.DebugCube->Server_SetVisibility(false);
			EntryIn.bIsVisible = false;
		}
	}
}

bool AVisualizeWorkerColors::InitGrid2D_Validate()
{
	return true;
}

void AVisualizeWorkerColors::InitGrid2D_Implementation()
{
	const USpatialGDKSettings* const GDKSettings = GetDefault<USpatialGDKSettings>();

	const int Width    = GDKSettings->WorldDimensionX / GDKSettings->ChunkEdgeLength;
	const int Height   = GDKSettings->WorldDimensionZ / GDKSettings->ChunkEdgeLength;

	const float Offset = GDKSettings->ChunkEdgeLength / 2.0f * 100;
	const float Scalar = GDKSettings->ChunkEdgeLength * 100; // meters to centimeters

	Grid2D.SetNum(Width * Height);

	int index = 0;
	for (int i = Width / 2 * -1; i < Width / 2; ++i)
	{
		for (int j = Height / 2 * -1; j < Height / 2; ++j)
		{
			Grid2D[index].SpawnPosition = FVector(i * Scalar + Offset, j * Scalar + Offset, 50);
			Grid2D[index].DebugCube     = nullptr;
			++index;
		}
	}
}

void AVisualizeWorkerColors::CompareChuncks(const int CenterCell, TArray<uint32> CompareTo)
{
	bool bVisibility = false;
	for (auto& EntryIn : CompareTo)
	{
		if (EntryIn < (uint32)Grid2D.Num())
		{
			if (!AreColorsMatching(CenterCell, EntryIn))
			{
				Grid2D[EntryIn].DebugCube->Server_SetVisibility(true);
				bVisibility = true;
			}
			else
			{
				if (IsOutterCube(CenterCell))
				{
					bVisibility = true;
				}
			}
		}
	}
	Grid2D[CenterCell].bIsVisible = bVisibility;
	Grid2D[CenterCell].DebugCube->Server_SetVisibility(bVisibility);
}

bool AVisualizeWorkerColors::SpawnBoundaryCubes_Validate()
{
	return true;
}

void AVisualizeWorkerColors::SpawnBoundaryCubes_Implementation()
{
	const USpatialGDKSettings* const GDKSettings = GetDefault<USpatialGDKSettings>();
	const int Width  = GDKSettings->WorldDimensionX / GDKSettings->ChunkEdgeLength;

	int CubesSpawnedThisRun = 0;

	for (; LastSpawnedIndex < Grid2D.Num(); ++LastSpawnedIndex, ++CubesSpawnedThisRun)
	{
		if (CubesSpawnedThisRun >= GDKSettings->CubesToSpawnAtATime)
		{
			break;
		}

		Grid2D[LastSpawnedIndex].DebugCube = GetWorld()->SpawnActorDeferred<ABoundaryCube>(ABoundaryCube::StaticClass(), FTransform(Grid2D[LastSpawnedIndex].SpawnPosition));
		Grid2D[LastSpawnedIndex].DebugCube->SetGridIndex(LastSpawnedIndex);
		Grid2D[LastSpawnedIndex].bIsVisible = true;
		Grid2D[LastSpawnedIndex].DebugCube->FinishSpawning(FTransform(Grid2D[LastSpawnedIndex].SpawnPosition));
	}

	if (LastSpawnedIndex < Grid2D.Num() - 1)
	{
		FTimerHandle UnusedHandle;
		GetWorldTimerManager().SetTimer(UnusedHandle, this, &AVisualizeWorkerColors::SpawnBoundaryCubes, GDKSettings->DelayToSpawnNextGroup, false);
	}
	else
	{
		FTimerHandle UnusedHandle;
		GetWorldTimerManager().SetTimer(UnusedHandle, this, &AVisualizeWorkerColors::BeginCreatingWalls, 2.0f, false);
	}
}

bool AVisualizeWorkerColors::IsOutterCube(const int CenterCell)
{
	const USpatialGDKSettings* const GDKSettings = GetDefault<USpatialGDKSettings>();
	const int Width  = GDKSettings->WorldDimensionX / GDKSettings->ChunkEdgeLength;
	return (CenterCell - Width) < 0 || CenterCell >= (Grid2D.Num() - Width) || CenterCell % Width == 0 || (CenterCell + 1) % Width == 0;
}
