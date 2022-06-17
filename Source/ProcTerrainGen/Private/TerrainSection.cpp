	// Fill out your copyright notice in the Description page of Project Settings.

#include "TerrainSection.h"
#include "LandscapeGenerator.h"
#include "DiskSampler.h"


int CalcIndexFromGridPos(const FIntPoint& gridSize, int x, int y)
{
	return x + y * gridSize.X;
}

FVector2D CalculateWorldCoordinatesFromTerrainCoords(const FIntPoint& TerrainCoords, const FVector2D& SectionSize)
{
	FVector2D WorldCoords;
	WorldCoords.X = TerrainCoords.X * SectionSize.X;
	WorldCoords.Y = TerrainCoords.Y * SectionSize.Y;

	return WorldCoords;
}

FVector UTerrainSection::CalculateVertexPosition(float xPos, float yPos)
{
	FVector vertPosition(xPos, yPos, 0);
	vertPosition += mMeshOrigin;

	float RawNoiseValue = USimplexNoiseBPLibrary::GetSimplexNoise2D_EX(vertPosition.X * mNoiseScale, vertPosition.Y * mNoiseScale, mLacunarity, mPersistance, mOctaves, 1.0f, true);
	vertPosition += FVector(0, 0, mLandscapeGen->ApplyTerrainHeightMultiplier(RawNoiseValue) * mHeightScale);

	return vertPosition;
}

bool UTerrainSection::IsOriginCoord(const FVector& PlayerLocation)
{
	return mLandscapeGen->GetCurrentGridPoint(PlayerLocation) == mTerrainCoords;
}

void UTerrainSection::InitialiseSection(ALandscapeGenerator* LandscapeGen , int SectionGridIndex, FIntPoint TerrainCoords, uint32 Seed, const FVector2D& SectionSize, const FIntPoint& ComponentsPerAxis, float fNoiseScale, float fHeightScale, float fLacunarity, float fPersistance, int Octaves)
{
	mLandscapeGen = LandscapeGen;
	mSectionIndex = SectionGridIndex;
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

	Thread = new FGeneratorThread(this);
	Thread->StartOperation(THREAD_OPERATION::GEN_LANDSCAPE);
}

void UTerrainSection::GenerateSectionMeshData()
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
			mSectionVertices.Add(CalculateVertexPosition(xPos, yPos));

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
			FVector vertex1;
			FVector vertex2;
			FVector vertex3;
			FVector vertex4;

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

			FVector dir1 = vertex2 - vertex1;
			FVector dir2 = vertex3 - vertex1;
			FVector normal1 = FVector::CrossProduct(dir2, dir1).GetSafeNormal();

			dir1 = vertex3 - vertex1;
			dir2 = vertex4 - vertex1;
			FVector normal2 = FVector::CrossProduct(dir2, dir1).GetSafeNormal();

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

	Points = NewObject<UDiskSampler>(GetTransientPackage(), UDiskSampler::StaticClass());
	Points->GeneratePoints(mSectionSize.X, mSectionSize.Y, 500, 15);

	bMeshGenerated = true;
}

bool UTerrainSection::GenerateLODData(int LOD)
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
			int vertindex = CalcIndexFromGridPos(mComponentsPerAxis + FIntPoint(1,1), i, j);
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

void UTerrainSection::UpdateTerrainSection(int LOD)
{
	if (mLandscapeGen->mesh && bMeshGenerated)
	{
		if (LOD > 4)
			return;

		if (LODLevel == LOD)
			return;

		if (LOD > 0 && GenLOD != LOD)
		{
			GenLOD = LOD;
			GeneratingLOD = true;
			Thread->StartOperation(GEN_LOD);
			return;
		}
			
		if (GeneratingLOD == false)
		{
			LODLevel = LOD;
			if (LODLevel > 0)
				mLandscapeGen->mesh->CreateMeshSection_LinearColor(mSectionIndex, mSectionLODVertices, mSectionLODIndices, mSectionLODNormals, TArray<FVector2D>(), TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
			else
				mLandscapeGen->mesh->CreateMeshSection_LinearColor(mSectionIndex, mSectionVertices, mSectionIndices, mSectionNormals, TArray<FVector2D>(), TArray<FLinearColor>(), TArray<FProcMeshTangent>(), true);
		}
		
		if (mLandscapeGen->GetMaterial())
			mLandscapeGen->mesh->SetMaterial(mSectionIndex, mLandscapeGen->GetMaterial());

	}
}

void UTerrainSection::RemoveSection()
{
	if(mLandscapeGen->mesh)
		mLandscapeGen->mesh->ClearMeshSection(mSectionIndex);

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

FGeneratorThread::FGeneratorThread(UTerrainSection* Section)
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
	}
	
	return 1;
}

void FGeneratorThread::Stop()
{
	bRunThread = false;
}