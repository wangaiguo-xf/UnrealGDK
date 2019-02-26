#include "VisualizeWorkerColors.h"
#include "BoundaryCube.h"
#include "UnrealNetwork.h"
#pragma  optimize("", off)
#define CHUNCK_EDGE_LENGTH 5
#define WORLD_DIMENSION_X  200
#define WORLD_DIMENSION_Z  200

AVisualizeWorkerColors::AVisualizeWorkerColors()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates          = true;
	ObjectColorsInWorker = FColor::MakeRandomColor();
}

void AVisualizeWorkerColors::BeginPlay()
{
	Super::BeginPlay();
	if (Role == ROLE_Authority)
	{
		InitGrid2D();
		FTimerHandle UnusedHandle;
		GetWorldTimerManager().SetTimer(UnusedHandle, this, &AVisualizeWorkerColors::SpawnBoundaryCubes, 5.0f, false);
	}
}

void AVisualizeWorkerColors::OnAuthorityGained()
{
	ABoundaryCube::BoundaryCubeOnAuthorityGained.BindUObject(this, &AVisualizeWorkerColors::OnBoundaryCubeOnAuthorityGained);
}

void AVisualizeWorkerColors::OnAuthorityLost()
{
	ABoundaryCube::BoundaryCubeOnAuthorityGained.Unbind();
}

void AVisualizeWorkerColors::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

bool AVisualizeWorkerColors::OnBoundaryCubeOnAuthorityGained_Validate(int InGridIndex, ABoundaryCube* InBoundaryCube)
{
	return true;
}

void AVisualizeWorkerColors::OnBoundaryCubeOnAuthorityGained_Implementation(int InGridIndex, ABoundaryCube* InBoundaryCube)
{
	if (InGridIndex < 0)
	{
		static int counter = 0;
		++counter;
		UE_LOG(LogTemp, Warning, TEXT("PlayInEditor: %d, Received -1 InGridIndex. Counter: %d"), GPlayInEditorID, counter);
		return;
	}
	Grid2D[InGridIndex].DebugCube = InBoundaryCube;

	for (auto& EntryIn : Grid2D)
	{
		if (!EntryIn.DebugCube.IsValid())
		{
			return;
		}
	}

	UpdateCubeVisibility();
	BuildBoundaryWalls();
}

void AVisualizeWorkerColors::UpdateCubeVisibility()
{
	for (int i = 0; i < Grid2D.Num(); ++i)
	{
		const uint32 Width = WORLD_DIMENSION_X / CHUNCK_EDGE_LENGTH;

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

void AVisualizeWorkerColors::BuildBoundaryWalls()
{
	const FRotator DefaultRotator{ 0, 0, 0 };
	
	TArray<FTransform> WallSpawnData;

	for (int EndIndex = 0; EndIndex < Grid2D.Num(); ++EndIndex)
	{
		const int StartIndex = EndIndex;

		const FDebugBoundaryInfo InfoStart = Grid2D[StartIndex];

		while (EndIndex < Grid2D.Num() && InfoStart.DebugCube->GetCurrentMeshColor() == Grid2D[EndIndex].DebugCube->GetCurrentMeshColor() && Grid2D[EndIndex].DebugCube->GetIsVisible())
		{
			++EndIndex;
		}
		
		if (StartIndex < EndIndex - 1)
		{
			--EndIndex;
			const FDebugBoundaryInfo InfoEnd = Grid2D[EndIndex];
			WallSpawnData.Add(FTransform(DefaultRotator, (InfoStart.Position + InfoEnd.Position) / 2.0f, FVector(1, FVector::Dist2D(InfoStart.Position, InfoEnd.Position) / 100.0f + 1, 1)));
		}
	}

	const int Width = WORLD_DIMENSION_X / CHUNCK_EDGE_LENGTH;
	TSet<int> IndexesUsed;

	for (int StartIndex = 0; StartIndex < Grid2D.Num(); ++StartIndex)
	{
		int EndIndex = StartIndex;

		const FDebugBoundaryInfo InfoStart = Grid2D[EndIndex];

		while (EndIndex < Grid2D.Num() && (InfoStart.DebugCube->GetCurrentMeshColor() == Grid2D[EndIndex].DebugCube->GetCurrentMeshColor()) && Grid2D[EndIndex].DebugCube->GetIsVisible() && !IndexesUsed.Find(EndIndex))
		{
			IndexesUsed.Add(EndIndex);
			EndIndex += Width;
		}

		EndIndex -= Width;
		if (StartIndex < EndIndex)
		{
			const FDebugBoundaryInfo InfoEnd = Grid2D[EndIndex];
			WallSpawnData.Add(FTransform(DefaultRotator, (InfoStart.Position + InfoEnd.Position) / 2.0f, FVector(FVector::Dist2D(InfoStart.Position, InfoEnd.Position) / 100.0f + 1, 1, 1)));
		}
	}

	for (auto& EntryIn : WallSpawnData)
	{
		ABoundaryCube* DebugCube = GetWorld()->SpawnActor<ABoundaryCube>(ABoundaryCube::StaticClass(), EntryIn);
		DebugCube->Server_SetVisibility(true);
	}

	TurnOffAllCubeVisibility();
}

void AVisualizeWorkerColors::TurnOffAllCubeVisibility()
{
	for (auto& EntryIn : Grid2D)
	{
		EntryIn.DebugCube->Server_SetVisibility(false);
	}
}

bool AVisualizeWorkerColors::InitGrid2D_Validate()
{
	return true;
}

void AVisualizeWorkerColors::InitGrid2D_Implementation()
{
	const int Width    = WORLD_DIMENSION_X / CHUNCK_EDGE_LENGTH;
	const int Height   = WORLD_DIMENSION_Z / CHUNCK_EDGE_LENGTH;

	const float Offset = CHUNCK_EDGE_LENGTH / 2.0f * 100;
	const float Scalar = CHUNCK_EDGE_LENGTH * 100; // meters to centimeters

	Grid2D.SetNum(Width * Height);

	int index = 0;
	for (int i = Width / 2 * -1; i < Width / 2; ++i)
	{
		for (int j = Height / 2 * -1; j < Height / 2; ++j)
		{
			Grid2D[index].Position  = FVector(i * Scalar + Offset, j * Scalar + Offset, 50);
			Grid2D[index].DebugCube = nullptr;
			++index;
		}
	}
}

void AVisualizeWorkerColors::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AVisualizeWorkerColors, Grid2D);
}

void AVisualizeWorkerColors::CompareChuncks(const int CenterCell, TArray<uint32> CompareTo)
{
	bool bVisibility = false;
	for (auto& EntryIn : CompareTo)
	{
		if (EntryIn < (uint32)Grid2D.Num())
		{
			if (Grid2D[CenterCell].DebugCube->GetCurrentMeshColor() != Grid2D[EntryIn].DebugCube->GetCurrentMeshColor())
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

	Grid2D[CenterCell].DebugCube->Server_SetVisibility(bVisibility);
}

bool AVisualizeWorkerColors::SpawnBoundaryCubes_Validate()
{
	return true;
}

void AVisualizeWorkerColors::SpawnBoundaryCubes_Implementation()
{
	static int Index = 0;

	int CubesSpawnedThisRun = 0;

	for (; Index < Grid2D.Num(); ++Index, ++CubesSpawnedThisRun)
	{
		ABoundaryCube* const DebugCube = GetWorld()->SpawnActorDeferred<ABoundaryCube>(ABoundaryCube::StaticClass(), FTransform(Grid2D[Index].Position));
		DebugCube->SetGridIndex(Index);
		DebugCube->FinishSpawning(FTransform(Grid2D[Index].Position));

		if (CubesSpawnedThisRun >= WORLD_DIMENSION_X / CHUNCK_EDGE_LENGTH)
		{
			break;
		}
	}

	if (Index < Grid2D.Num() - 1)
	{
		FTimerHandle UnusedHandle;
		GetWorldTimerManager().SetTimer(UnusedHandle, this, &AVisualizeWorkerColors::SpawnBoundaryCubes, 1.0f, false);
	}
	else
	{
		Index = 0;
	}
}

bool AVisualizeWorkerColors::IsOutterCube(const int CenterCell)
{
	const int Width = WORLD_DIMENSION_X  / CHUNCK_EDGE_LENGTH;
	bool bReturn    = false;

	// top row and bottom row
	if ((CenterCell - Width) < 0 || CenterCell >= (Grid2D.Num() - Width))
	{
		bReturn = true;
	}
	// first column and last column
	if (CenterCell % Width == 0 || (CenterCell + 1) % Width == 0)
	{
		bReturn = true;
	}

	return bReturn;
}
