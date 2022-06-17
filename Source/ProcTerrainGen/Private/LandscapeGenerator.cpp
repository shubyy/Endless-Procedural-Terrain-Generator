// Fill out your copyright notice in the Description page of Project Settings.


#include "LandscapeGenerator.h"
#include "KismetProceduralMeshLibrary.h"

#define LOCTEXT_NAMESPACE "Terrain"

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
	UTerrainSection* SectionsToRemove = nullptr;
	for (UTerrainSection* SectionObject : SectionObjects)
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
		UTerrainSection* Section = DoesTerrainCoordExist(Coord);
		if (!Section)
		{
			//Find Free section
			int SectionIndex = FindAndReserveFreeSectionIndex();
			if (SectionIndex < 0)
			{
				if (SectionsToRemove)
				{
					//Remove designated section
					FString debugtxt = FText::Format(LOCTEXT("Rem", "Removing terrain coord ({0},{1})"), SectionsToRemove->mTerrainCoords.X, SectionsToRemove->mTerrainCoords.Y).ToString();
					GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, debugtxt);
					SectionsToRemove->RemoveSection();
					FreeSectionIndex(SectionsToRemove->mSectionIndex);
					SectionObjects.RemoveSingleSwap(SectionsToRemove);
					SectionsToRemove = nullptr;

					SectionIndex = FindAndReserveFreeSectionIndex();
				}
			}

			if (SectionIndex >= 0)
			{
				//Create terrain object
				FString debugtxt = FText::Format(LOCTEXT("Gen", "Generating terrain coord ({0},{1})"), Coord.X, Coord.Y).ToString();
				GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, debugtxt);

				//Calc LOD Level;
				int LODLevel = CalcLODLevelFromTerrainCoordDistance((Coord - CurrentGridCoord).Size());

				UTerrainSection* NewSection = NewObject<UTerrainSection>(GetTransientPackage(), UTerrainSection::StaticClass());
				SectionObjects.Add(NewSection);

				NewSection->InitialiseSection(this, SectionIndex, Coord, NoiseSeed, LandscapeSectionSize, LandscapeComponentSize, fNoiseScale, fHeightScale, fLacunarity, fPersistance, Octaves);
				NewSection->UpdateTerrainSection(0);

				Generated = true;
				mGenerating = true;

				break;
			}
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

UTerrainSection* ALandscapeGenerator::DoesTerrainCoordExist(const FIntPoint& TerrainCoord)
{
	for (UTerrainSection* SectionObject : SectionObjects)
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

int ALandscapeGenerator::FindAndReserveFreeSectionIndex()
{
	for (int i = 0; i < ReservedSections.Num(); i++)
	{
		if (!ReservedSections[i])
		{
			ReservedSections[i] = true;
			return i;
		}
	}
	return -1;
}

bool ALandscapeGenerator::IsCloseForCollision(const FIntPoint& Coords, const FVector& PlayerLocation)
{
	FVector2D WorldCenterCoord = CalculateWorldCoordinatesFromTerrainCoords(Coords, LandscapeSectionSize) + (LandscapeSectionSize / 2.0f);
	float dist = FVector2D::Distance(WorldCenterCoord, FVector2D(PlayerLocation));
	float MaxDist = LandscapeSectionSize.Length() / 1.95f;

	return dist < MaxDist;
}

void ALandscapeGenerator::FreeSectionIndex(int index)
{
	ReservedSections[index] = false;
}

inline int ALandscapeGenerator::CalcLODLevelFromTerrainCoordDistance(float Distance)
{
	return FMath::Floor(Distance / 2.0f);
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

	mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("LandscapeMesh"));
	RootComponent = mesh;
	mesh->bUseAsyncCooking = true;
	mesh->SetMobility(EComponentMobility::Static);
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

	mGenerating = false;
	bCanGenerate = false;
}

UMaterialInterface* ALandscapeGenerator::GetMaterial()
{
	return LandscapeMat;
}

void ALandscapeGenerator::PostActorCreated()
{
	Super::PostActorCreated();
	
}

void ALandscapeGenerator::StartGeneration()
{
	bCanGenerate = true;
}

// Called when the game starts or when spawned
void ALandscapeGenerator::BeginPlay()
{
	for (int i = 0; i < MaxSectionCount; i++)
	{
		ReservedSections.Add(false);
	}

	Super::BeginPlay();
}

// Called every frame
void ALandscapeGenerator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if(bCanGenerate)
		GenerateNewTerrainGrid();
}

