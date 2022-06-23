// Fill out your copyright notice in the Description page of Project Settings.


#include "LandscapeSection.h"
#include "LandscapeGenerator.h"
#include "SimplexNoise/Public/SimplexNoiseBPLibrary.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/RuntimeMeshComponentStatic.h"
#include "Providers/RuntimeMeshProviderStatic.h"
#include "DiskSampler.h"

#define LOCTEXT_NAMESPACE "Section"


// Sets default values
ALandscapeSection::ALandscapeSection()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	FoliageGenerated = false;
	bMeshGenerated = false;
	PointsGenerated = false;
	CollisionGenerated = false;
	GeneratingCollision = false;
	Points = nullptr;

	mesh = CreateDefaultSubobject<URuntimeMeshComponent>(TEXT("LandscapeMesh"));
	RootComponent = mesh;
	mesh->SetMobility(EComponentMobility::Static);

	InstMesh = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("HierarchicalInstancedStaticMeshComponent"));
	InstMesh->SetCollisionProfileName("BlockAllDynamic");
	InstMesh->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
	InstMesh->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
}

// Called when the game starts or when spawned
void ALandscapeSection::BeginPlay()
{
	Super::BeginPlay();

	StaticProvider = NewObject<URuntimeMeshProviderStatic>(this, TEXT("StaticProvider"));
	CollisionProvider = NewObject<URuntimeMeshProviderCollision>(this, TEXT("CollisionProvider"));

	CollisionProvider->SetChildProvider(StaticProvider);
	CollisionProvider->SetRenderableSectionAffectsCollision(0, true);

	FRuntimeMeshCollisionSettings Settings;
	Settings.bUseAsyncCooking = true;
	Settings.bUseComplexAsSimple = true;
	Settings.CookingMode = ERuntimeMeshCollisionCookingMode::CookingPerformance;
	CollisionProvider->SetCollisionSettings(Settings);

	mesh->Initialize(CollisionProvider);
}

int CalcIndexFromGridPos(const FIntPoint& gridSize, int x, int y)
{
	return x + y * gridSize.X;
}

FVector3f ALandscapeSection::CalculateVertexPosition(float xPos, float yPos)
{
	FVector3f vertPosition(xPos, yPos, 0.0f);
	vertPosition += FVector3f(mMeshOrigin);

	float RawNoiseValue = USimplexNoiseBPLibrary::GetSimplexNoise2D_EX(vertPosition.X * mNoiseScale, vertPosition.Y * mNoiseScale, mLacunarity, mPersistance, mOctaves, 1.0f, true);
	vertPosition += FVector3f(0.0, 0.0, mLandscapeGen->ApplyTerrainHeightMultiplier(RawNoiseValue) * mHeightScale);

	return vertPosition;
}

void ALandscapeSection::GenerateFoliage()
{

	if (!bMeshGenerated || !PointsGenerated)
		return;

	FoliageGenerated = true;
	if (!mLandscapeGen->AddFoliage)
		return;
	
	FCollisionQueryParams Params;
	Params.bTraceComplex = true;
	Params.AddIgnoredComponent(InstMesh);
	FHitResult Res;
	
	for (FVector2D Point : Points->PointList)
	{
		FVector Start = FVector(Point, mHeightScale + 3000.0f) + mMeshOrigin;
		FVector End = FVector(Point, -3000.0f) + mMeshOrigin;
		
		bool bHit = GetWorld()->LineTraceSingleByChannel(Res, Start, End, ECollisionChannel::ECC_Visibility, Params);
		if (bHit)
		{
			if (Res.ImpactPoint.Z < 6000)
			{
				FTransform InstTransform(Res.ImpactPoint);
				InstMesh->AddInstance(InstTransform, true);
			}
		}
	}

	InstMesh->SetMobility(EComponentMobility::Static);
}

void ALandscapeSection::RemoveFoliage()
{
	InstMesh->ClearInstances();
	FoliageGenerated = false;
}

bool ALandscapeSection::IsOriginCoord(const FVector& PlayerLocation)
{
	return mLandscapeGen->GetCurrentGridPoint(PlayerLocation) == mTerrainCoords;
}

void ALandscapeSection::InitialiseSection(ALandscapeGenerator* LandscapeGen, FIntPoint TerrainCoords, uint32 Seed, const FVector2D& SectionSize, const FIntPoint& ComponentsPerAxis, float fNoiseScale, float fHeightScale, float fLacunarity, float fPersistance, int Octaves)
{
	mLandscapeGen = LandscapeGen;
	mTerrainCoords = TerrainCoords;

	bMeshGenerated = false;
	LODLevel = -1;
	GenLOD = -1;

	mSectionSize = SectionSize;
	mComponentsPerAxis = ComponentsPerAxis;
	mHeightScale = fHeightScale;
	mNoiseScale = fNoiseScale;
	mLacunarity = fLacunarity;
	mPersistance = fPersistance;
	mOctaves = Octaves;
	GlobalSeed = Seed;

	UStaticMesh* treeMesh = mLandscapeGen->TreeMesh;
	if (treeMesh)
		InstMesh->SetStaticMesh(treeMesh);

	InstMesh->SetMobility(EComponentMobility::Static);

	Thread = new FGeneratorThread(this);
	Thread->StartOperation(THREAD_OPERATION::GEN_LANDSCAPE);

}

void ALandscapeSection::GenerateSectionMeshData()
{
	FVector MeshOrigin = FVector(CalculateWorldCoordinatesFromTerrainCoords(mTerrainCoords, mSectionSize), 0.0);
	mMeshOrigin = MeshOrigin;
	float rowVertDist = mSectionSize.Y / mComponentsPerAxis.Y;
	float columnVertDist = mSectionSize.X / mComponentsPerAxis.X;

	FIntPoint OverallComponents = mComponentsPerAxis + FIntPoint(1, 1);

	//Generate Vertices
	for (int j = 0; j < OverallComponents.Y; j++)
	{
		float yPos = rowVertDist * j;
		for (int i = 0; i < OverallComponents.X; i++)
		{
			//Generate vertex
			float xPos = columnVertDist * i;
			FVector3f VertexPos = CalculateVertexPosition(xPos, yPos);
			mSectionVertices.Add(VertexPos);

			//Generate Indices
			if ((i < OverallComponents.X - 1) && (j < OverallComponents.Y - 1))
			{
				//Generate Triangle Index
				int index11 = CalcIndexFromGridPos(OverallComponents, i, j);
				int index12 = CalcIndexFromGridPos(OverallComponents, i, j + 1);
				int index13 = CalcIndexFromGridPos(OverallComponents, i + 1, j + 1);

				int index22 = CalcIndexFromGridPos(OverallComponents, i + 1, j + 1);
				int index23 = CalcIndexFromGridPos(OverallComponents, i + 1, j);

				mSectionIndices.Add(index11);
				mSectionIndices.Add(index12);
				mSectionIndices.Add(index13);

				mSectionIndices.Add(index11);
				mSectionIndices.Add(index22);
				mSectionIndices.Add(index23);
			}
		}
	}

	//Use custom method to generate normals to fix seams.
	mSectionNormals.SetNumZeroed(mSectionVertices.Num());
	for (int j = -1; j < OverallComponents.Y; j++)
	{
		for (int i = -1; i < OverallComponents.X; i++)
		{
			FVector3f vertex1;
			FVector3f vertex2;
			FVector3f vertex3;
			FVector3f vertex4;

			int index11 = CalcIndexFromGridPos(OverallComponents, i, j);
			int index12 = CalcIndexFromGridPos(OverallComponents, i, j + 1);
			int index13 = CalcIndexFromGridPos(OverallComponents, i + 1, j + 1);
			int index23 = CalcIndexFromGridPos(OverallComponents, i + 1, j);

			if ((i > -1 && i < OverallComponents.X - 1) && (j > -1 && j < OverallComponents.Y - 1))
			{
				vertex1 = mSectionVertices[index11];
				vertex2 = mSectionVertices[index12];
				vertex3 = mSectionVertices[index13];
				vertex4 = mSectionVertices[index23];
			}
			else
			{
				float xPos1 = columnVertDist * i;
				float yPos1 = rowVertDist * j;
				float yPos2 = rowVertDist * (j + 1);
				float xPos2 = columnVertDist * (i + 1);

				vertex1 = CalculateVertexPosition(xPos1, yPos1);
				vertex2 = CalculateVertexPosition(xPos1, yPos2);
				vertex3 = CalculateVertexPosition(xPos2, yPos2);
				vertex4 = CalculateVertexPosition(xPos2, yPos1);
			}

			FVector3f dir1 = vertex2 - vertex1;
			FVector3f dir2 = vertex3 - vertex1;
			FVector3f normal1 = FVector3f::CrossProduct(dir2, dir1).GetSafeNormal();

			dir1 = vertex3 - vertex1;
			dir2 = vertex4 - vertex1;
			FVector3f normal2 = FVector3f::CrossProduct(dir2, dir1).GetSafeNormal();

			if (i == -1)
			{
				if (j == -1)
				{
					mSectionNormals[index13] += normal1;
					mSectionNormals[index13] += normal2;
				}
				else if (j == OverallComponents.Y - 1)
					mSectionNormals[index23] += normal2;
				else
				{
					mSectionNormals[index13] += normal1;
					mSectionNormals[index13] += normal2;
					mSectionNormals[index23] += normal2;
				}
			}
			else if (j == -1)
			{
				mSectionNormals[index12] += normal1;
				if (i != OverallComponents.X - 1)
				{
					mSectionNormals[index13] += normal1;
					mSectionNormals[index13] += normal2;
				}
			}
			else if (i == OverallComponents.X - 1)
			{
				mSectionNormals[index11] += normal1;
				mSectionNormals[index11] += normal2;
				if (j != OverallComponents.Y - 1)
					mSectionNormals[index12] += normal1;
			}
			else if (j == OverallComponents.Y - 1)
			{
				mSectionNormals[index11] += normal1;
				mSectionNormals[index11] += normal2;
				if (i != OverallComponents.X - 1)
					mSectionNormals[index23] += normal2;
			}
			else
			{
				mSectionNormals[index11] += normal1;
				mSectionNormals[index11] += normal2;

				mSectionNormals[index12] += normal1;
				mSectionNormals[index13] += normal1;

				mSectionNormals[index13] += normal2;
				mSectionNormals[index23] += normal2;
			}

		}
	}

	//Normalize normals
	for (int i = 0; i < mSectionNormals.Num(); i += 1)
		mSectionNormals[i].Normalize();

	bMeshGenerated = true;

	Points = NewObject<UDiskSampler>(GetTransientPackage(), UDiskSampler::StaticClass());

	int64 seed = (GlobalSeed % (mTerrainCoords.X == 0 ? 50 : mTerrainCoords.X)) + mTerrainCoords.Y;

	Points->GeneratePoints(seed, mSectionSize.X, mSectionSize.Y, 1100, 10);
	PointsGenerated = true;
}

bool ALandscapeSection::GenerateCollisionFromLOD(int LOD)
{
	if (!bMeshGenerated)
		return false;

	int Skip = FMath::Floor(FMath::Pow(2, LOD));

	FIntPoint ActualComponents = mComponentsPerAxis / Skip + FIntPoint(1, 1);

	for (int j = 0; j < ActualComponents.Y; j++)
	{
		for (int i = 0; i < ActualComponents.X; i++)
		{
			int vertindex = CalcIndexFromGridPos(mComponentsPerAxis + FIntPoint(1, 1), i, j);
			CollisionData.Vertices.Add(mSectionVertices[vertindex * Skip]);

			if ((i < ActualComponents.X - 1) && (j < ActualComponents.Y - 1))
			{
				int index11 = CalcIndexFromGridPos(ActualComponents, i, j);
				int index12 = CalcIndexFromGridPos(ActualComponents, i, j + 1);
				int index13 = CalcIndexFromGridPos(ActualComponents, i + 1, j + 1);

				int index22 = CalcIndexFromGridPos(ActualComponents, i + 1, j + 1);
				int index23 = CalcIndexFromGridPos(ActualComponents, i + 1, j);

				CollisionData.Triangles.Add(index11, index12, index13);
				CollisionData.Triangles.Add(index11, index22, index23);
			}
		}
	}
	
	CollisionGenerated = true;
	GeneratingCollision = false;
	return true;
}

bool ALandscapeSection::GenerateLODData(int LOD)
{
	GeneratingLOD = true;

	if (!bMeshGenerated)
		return false;

	mSectionLODVertices.Empty();
	mSectionLODNormals.Empty();
	mSectionLODIndices.Empty();

	int Skip = FMath::Floor(FMath::Pow(2, LOD));

	FIntPoint ActualComponents = mComponentsPerAxis / Skip + FIntPoint(1, 1);

	for (int j = 0; j < ActualComponents.Y; j++)
	{
		for (int i = 0; i < ActualComponents.X; i++)
		{
			int vertindex = CalcIndexFromGridPos(mComponentsPerAxis + FIntPoint(1, 1), i, j);
			mSectionLODVertices.Add(mSectionVertices[vertindex * Skip]);
			mSectionLODNormals.Add(mSectionNormals[vertindex * Skip]);

			if ((i < ActualComponents.X - 1) && (j < ActualComponents.Y - 1))
			{
				int index11 = CalcIndexFromGridPos(ActualComponents, i, j);
				int index12 = CalcIndexFromGridPos(ActualComponents, i, j + 1);
				int index13 = CalcIndexFromGridPos(ActualComponents, i + 1, j + 1);

				int index22 = CalcIndexFromGridPos(ActualComponents, i + 1, j + 1);
				int index23 = CalcIndexFromGridPos(ActualComponents, i + 1, j);

				mSectionLODIndices.Add(index11);
				mSectionLODIndices.Add(index12);
				mSectionLODIndices.Add(index13);

				mSectionLODIndices.Add(index11);
				mSectionLODIndices.Add(index22);
				mSectionLODIndices.Add(index23);
			}
		}
	}

	GeneratingLOD = false;
	return true;
}

void ALandscapeSection::UpdateTerrainSection(int LOD)
{
	if (mesh && bMeshGenerated && !GeneratingCollision)
	{
		if (LOD > 4 || LODLevel == LOD)
			return;

		if (LOD > 0 && GenLOD != LOD)
		{
			GenLOD = LOD;
			GeneratingLOD = true;
			Thread->StartOperation(GEN_LOD);
			return;
		}

		if (!CollisionGenerated)
		{
			GeneratingCollision = true;
			Thread->StartOperation(GEN_COLLISION);
			return;
		}

		CollisionProvider->SetCollisionMesh(CollisionData);

		if (!GeneratingLOD)
		{
			LODLevel = LOD;
			if (LODLevel > 0)
			{
				mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				StaticProvider->CreateSectionFromComponents(0, 0, 0, mSectionLODVertices, mSectionLODIndices, mSectionLODNormals, TArray<FVector2f>(), TArray<FColor>(), TArray<FRuntimeMeshTangent>(), ERuntimeMeshUpdateFrequency::Infrequent, false);
				if (FoliageGenerated)
					RemoveFoliage();
			}
			else
			{
				mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				StaticProvider->CreateSectionFromComponents(0, 0, 0, mSectionVertices, mSectionIndices, mSectionNormals, TArray<FVector2f>(), TArray<FColor>(), TArray<FRuntimeMeshTangent>(), ERuntimeMeshUpdateFrequency::Infrequent, false);
				if (!FoliageGenerated)
					GenerateFoliage();
			}
		}

		if (mLandscapeGen->GetMaterial())
			mesh->SetMaterial(0, mLandscapeGen->GetMaterial());

	}
}

void ALandscapeSection::RemoveSection()
{
	if (StaticProvider)
		StaticProvider->ClearSection(0, 0);

	mLandscapeGen = nullptr;
	if (Points)
	{
		Points->ConditionalBeginDestroy();
		Points = nullptr;
	}

	if (Thread)
	{
		delete Thread;
		Thread = nullptr;
	}
}

/*******************************************
MultiThreader
*******************************************/

FGeneratorThread::FGeneratorThread(ALandscapeSection* Section)
{
	Thread = nullptr;
	bRunThread = true;
	mSection = Section;
	mOperation = GEN_LANDSCAPE;
}

bool FGeneratorThread::StartOperation(THREAD_OPERATION operation)
{
	mOperation = operation;

	Thread = FRunnableThread::Create(this, TEXT("GeneratorThread"));
	return true;
}

FGeneratorThread::~FGeneratorThread()
{
	if (Thread)
	{
		Thread->Kill();
		delete Thread;
	}
}

bool FGeneratorThread::Init()
{
	return true;
}

uint32 FGeneratorThread::Run()
{
	switch (mOperation)
	{
	case GEN_LANDSCAPE:
		mSection->GenerateSectionMeshData();
		break;
	case GEN_LOD:
		mSection->GenerateLODData(mSection->GenLOD);
		break;
	case GEN_COLLISION:
		mSection->GenerateCollisionFromLOD(1);
		break;
	}

	return 1;
}

void FGeneratorThread::Stop()
{
	bRunThread = false;
}