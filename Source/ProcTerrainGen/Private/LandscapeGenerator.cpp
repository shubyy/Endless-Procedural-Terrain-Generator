// Fill out your copyright notice in the Description page of Project Settings.


#include "LandscapeGenerator.h"
#include "LandscapeSection.h"

#define LOCTEXT_NAMESPACE "Terrain"

FVector2D CalculateWorldCoordinatesFromTerrainCoords(const FIntPoint& TerrainCoords, const FVector2D& SectionSize)
{
	FVector2D WorldCoords;
	WorldCoords.X = TerrainCoords.X * SectionSize.X;
	WorldCoords.Y = TerrainCoords.Y * SectionSize.Y;

	return WorldCoords;
}

float ALandscapeGenerator::ApplyTerrainHeightMultiplier(float value)
{
	if (TerrainHeight)
		return TerrainHeight->GetFloatValue(value);

	return value;
}

FIntPoint ALandscapeGenerator::GetCurrentGridPoint(const FVector& PlayerLocation)
{
	FIntPoint CurrentGridCoord;
	CurrentGridCoord.X = FMath::Floor(PlayerLocation.X / LandscapeSectionSize.X);
	CurrentGridCoord.Y = FMath::Floor(PlayerLocation.Y / LandscapeSectionSize.Y);
	return CurrentGridCoord;
}

//Returns current grid point
FIntPoint ALandscapeGenerator::CalcVisibleGridPoints(const FVector& PlayerLocation)
{
	FIntPoint CurrentGridCoord = GetCurrentGridPoint(PlayerLocation);
	VisibleGridCoords.Empty();

	for (int i = -GenerationLevel; i <= GenerationLevel; i++)
	{
		for (int j = -GenerationLevel; j <= GenerationLevel; j++)
		{
			FIntPoint CurrentCoord = FIntPoint(i, j);
			VisibleGridCoords.Add(CurrentGridCoord + CurrentCoord);
		}
	}

	return CurrentGridCoord;
}

void ALandscapeGenerator::GenerateNewTerrainGrid()
{
	//Only one terrain can be generated and removed at the same time
	bool Generated = false;

	FVector PlayerLocation;
	APawn* CurrentPawn = GetWorld()->GetFirstPlayerController()->GetPawn();
	if (!CurrentPawn)
		PlayerLocation = FVector(0, 0, 0);
	else
		PlayerLocation = CurrentPawn->GetActorLocation();

	FIntPoint CurrentGridCoord = CalcVisibleGridPoints(PlayerLocation);

	//Find removable section
	ALandscapeSection* SectionsToRemove = nullptr;
	for (ALandscapeSection* SectionObject : SectionObjects)
	{
		if (!IsTerrainCoordVisible(SectionObject->mTerrainCoords))
		{
			SectionsToRemove = SectionObject;
			break;
		}
	}

	//For each coord
	for (auto Coord : VisibleGridCoords)
	{
		ALandscapeSection* Section = DoesTerrainCoordExist(Coord);
		if (!Section)
		{
			//Find Free section
			if (SectionsToRemove)
			{
				//Remove designated section
				//FString debugtxt = FText::Format(LOCTEXT("Rem", "Removing terrain coord ({0},{1})"), SectionsToRemove->mTerrainCoords.X, SectionsToRemove->mTerrainCoords.Y).ToString();
				//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, debugtxt);
				SectionsToRemove->RemoveSection();

				SectionObjects.RemoveSingleSwap(SectionsToRemove);
				SectionsToRemove->Destroy();
				SectionsToRemove = nullptr;
			}
			
			mGenerating = true;
			//Create terrain object
			//FString debugtxt = FText::Format(LOCTEXT("Gen", "Generating terrain coord ({0},{1})"), Coord.X, Coord.Y).ToString();
			//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, debugtxt);

			//Calc LOD Level;
			int LODLevel = CalcLODLevelFromTerrainCoordDistance((Coord - CurrentGridCoord).Size());

			//Spawn Actor
			FVector Pos = FVector(CalculateWorldCoordinatesFromTerrainCoords(Coord, LandscapeSectionSize), 0);
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ALandscapeSection* NewSection = GetWorld()->SpawnActor<ALandscapeSection>(ALandscapeSection::StaticClass(), FVector(0,0,0), FRotator::ZeroRotator, Params);
			
			SectionObjects.Add(NewSection);
			NewSection->InitialiseSection(this, Coord, NoiseSeed, LandscapeSectionSize, LandscapeComponentSize, fNoiseScale, fHeightScale, fLacunarity, fPersistance, Octaves);
			NewSection->UpdateTerrainSection(0);

			Generated = true;
			break;
		}
		else
		{
			//Calc LOD Level;
			int LODLevel = CalcLODLevelFromTerrainCoordDistance((Coord - CurrentGridCoord).Size());
			
			//Only update if LOD has changed
			if (Section->LODLevel != LODLevel)
				Section->UpdateTerrainSection(LODLevel);	
		}
	}

	if (!Generated)
	{
		if (mGenerating)
		{
			mGenerating = false;
			OnGenerated.Broadcast();
		}
	}
}

ALandscapeSection* ALandscapeGenerator::DoesTerrainCoordExist(const FIntPoint& TerrainCoord)
{
	for (ALandscapeSection* SectionObject : SectionObjects)
		if (SectionObject->mTerrainCoords == TerrainCoord)
			return SectionObject;

	return nullptr;
}

bool ALandscapeGenerator::IsTerrainCoordVisible(const FIntPoint& Coord)
{
	bool found = false;
	for (auto VisibleCoord : VisibleGridCoords)
	{
		if (VisibleCoord == Coord)
		{
			found = true;
			break;
		}	
	}

	return found;
}

bool ALandscapeGenerator::IsCloseForCollision(const FIntPoint& Coords, const FVector& PlayerLocation)
{
	FVector2D WorldCenterCoord = CalculateWorldCoordinatesFromTerrainCoords(Coords, LandscapeSectionSize) + (LandscapeSectionSize / 2.0f);
	float dist = FVector2D::Distance(WorldCenterCoord, FVector2D(PlayerLocation));
	float MaxDist = LandscapeSectionSize.Length() / 1.95f;

	return dist < MaxDist;
}

inline int ALandscapeGenerator::CalcLODLevelFromTerrainCoordDistance(float Distance)
{
	return FMath::Floor(Distance / 1.5f);
}

// Sets default values
ALandscapeGenerator::ALandscapeGenerator()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	NoiseSeed = FDateTime::Now().ToUnixTimestamp();

	PrimaryActorTick.bCanEverTick = true;
	//bAllowTickBeforeBeginPlay = false;
	//PrimaryActorTick.TickInterval = 0.5f;
	SetActorTickInterval( 0.5f);

	GenerationLevel = 1;
	
	LandscapeSectionSize = FVector2D(45000.0, 45000.0);
	LandscapeUVScale = FVector2D(5, 5);
	LandscapeComponentSize = FIntPoint(100, 100);
	MaxSectionCount = 9;

	fNoiseScale = 0.1f;
	fHeightScale = 1.0f;

	fLacunarity = 2.3f;
	fPersistance = 0.6f;
	Octaves = 4;

	AddFoliage = true;
	AddCollision = true;

	const ConstructorHelpers::FObjectFinder<UStaticMesh> MeshObj(TEXT("StaticMesh'/Game/Meshes/Sphere.Sphere'"));
	if(MeshObj.Succeeded())
		TreeMesh = MeshObj.Object;

	mGenerating = false;
	bCanGenerate = false;
}

UMaterialInterface* ALandscapeGenerator::GetMaterial()
{
	return LandscapeMat;
}


void ALandscapeGenerator::StartGeneration()
{
	bCanGenerate = true;
}

// Called when the game starts or when spawned
void ALandscapeGenerator::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void ALandscapeGenerator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if(bCanGenerate)
		GenerateNewTerrainGrid();
}

