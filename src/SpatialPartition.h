#pragma once

#include <algorithm>
#include <assert.h>
#include <vector>

#include "raymath.h"

template<typename TPayload>
class SpatialPartition {
public:
	void InitArea(Vector2 extents, int countX, int countY) {
		Extents = extents;
		CountX = countX;
		CountY = countY;
		mSparseCells.resize(CountX * CountY);
	}

	void Clear() {
		mPayloads.clear();
		mPackedCells.clear();
		//-------mSparseCells stays put
		mCellCounts.clear();
		mCellFirst.clear();
		mPartition.clear();
		mInsertionAreas.clear();
	}

	void InsertDeferred(TPayload payload, const Vector2& min, const Vector2& max) {
		assert(min.x <= max.x);
		assert(min.y <= max.y);

		mPayloads.push_back(payload);
		uint32_t minCellID = GetCellID(min);
		uint32_t maxCellID = GetCellID(max);

		mInsertionAreas.emplace_back(minCellID, maxCellID);
	}

	void FlushInsertions() {
		for (const InsertionArea& area : mInsertionAreas) {
			auto countAction = [this](uint32_t cellIt) {
				uint32_t cellIndex = mSparseCells[cellIt];
				bool exists = cellIndex < mPackedCells.size() && mPackedCells[cellIndex] == cellIt;
				if (!exists) {
					cellIndex = mPackedCells.size();
					mSparseCells[cellIt] = cellIndex;
					mPackedCells.push_back(cellIt);
					mCellCounts.push_back(0);
				}
				mCellCounts[cellIndex]++;
			};

			IterateCells(area.MinCellID, area.MaxCellID, countAction);
		}

		// Determine first index for each cell
		mCellFirst.resize(mCellCounts.size());
		uint32_t firstAcc = 0;
		auto firstIt = mCellFirst.begin();
		for (const uint32_t count : mCellCounts) {
			*firstIt = firstAcc;
			++firstIt;
			firstAcc += count;
		}

		mPartition.resize(firstAcc);
		// reset counts to 0 so we can insert again
		mCellCounts.assign(mPackedCells.size(), 0);

		uint32_t payloadId = 0;
		for (const InsertionArea& area : mInsertionAreas) {
			auto insertAction = [this, payloadId](uint32_t cellIt) {
				const uint32_t cellIndex = mSparseCells[cellIt];
				const uint32_t insertIndex = mCellFirst[cellIndex] + mCellCounts[cellIndex];
				mCellCounts[cellIndex] += 1;
				mPartition[insertIndex] = payloadId;
			};

			IterateCells(area.MinCellID, area.MaxCellID, insertAction);
			++payloadId;
		}
	}

private:
	uint32_t GetCellID(const Vector2& point) {
		float relativeX = std::clamp(point.x / Extents.x, 0.f, 1.f);
		float relativeY = std::clamp(point.y / Extents.y, 0.f, 1.f);
		int i = std::min(static_cast<int>(relativeX * CountX), CountX - 1);
		int j = std::min(static_cast<int>(relativeY * CountY), CountY - 1);
		return i + j * CountX;
	}

	template<typename TAction>
	void IterateCells(uint32_t minCell, uint32_t maxCell, TAction&& action)
	{
		uint32_t strideX = (maxCell % CountX) - (minCell % CountX);
		uint32_t strideY = (maxCell / CountX) - (minCell / CountX);

		uint32_t cellIt = minCell;
		for (uint32_t j = 0; j <= strideY; ++j) {
			for (uint32_t i = 0; i <= strideX; ++i) {
				action(cellIt + i);
			}
			cellIt += CountX;
		}
	}

	struct InsertionArea {
		uint32_t MinCellID;
		uint32_t MaxCellID;
	};

	Vector2 Extents;
	int CountX;
	int CountY;

	std::vector<TPayload> mPayloads;
	std::vector<uint32_t> mPackedCells;
	std::vector<uint32_t> mSparseCells;
	std::vector<uint32_t> mCellCounts;
	std::vector<uint32_t> mCellFirst;
	std::vector<uint32_t> mPartition;
	std::vector<InsertionArea> mInsertionAreas;
};