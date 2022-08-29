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
		auto [minI, minJ] = CellIntCoords(min);
		auto [maxI, maxJ] = CellIntCoords(max);

		Area area = { minI, minJ, maxI, maxJ };

		mInsertionAreas.push_back(area);
	}

	void FlushInsertions() {
		for (const Area& area : mInsertionAreas) {
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
			IterateArea(area, countAction);
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
		for (const Area& area : mInsertionAreas) {
			auto insertAction = [this, payloadId](uint32_t cellIt) {
				const uint32_t cellIndex = mSparseCells[cellIt];
				const uint32_t insertIndex = mCellFirst[cellIndex] + mCellCounts[cellIndex];
				mCellCounts[cellIndex] += 1;
				mPartition[insertIndex] = payloadId;
			};
			IterateArea(area, insertAction);
			++payloadId;
		}
	}

	template<typename TPairAction>
	void IteratePairs(TPairAction&& pairAction) {
		mPairAccumulator.clear();
		mPairAppend.clear();
		for (size_t cellIndex = 0; cellIndex < mPackedCells.size(); ++cellIndex) {
			const uint32_t cellFirst = mCellFirst[cellIndex];
			const uint32_t cellCount = mCellCounts[cellIndex];
			const size_t accumulatorSize = mPairAccumulator.size();
			size_t minBound = accumulatorSize;
			for (size_t firstIt = 0; firstIt < cellCount - 1; ++firstIt) {
				const uint32_t firstItem = mPartition[cellFirst + firstIt];
				for (size_t secondIt = firstIt + 1; secondIt < cellCount; ++secondIt) {
					const uint32_t secondItem = mPartition[cellFirst + secondIt];
					IndexPair pair = { firstItem, secondItem };
					auto lower = std::lower_bound(mPairAccumulator.begin(), mPairAccumulator.begin() + accumulatorSize, pair, PairCompare{});
					if (lower != mPairAccumulator.end() && *lower == pair) {
						continue;
					}
					minBound = std::min<size_t>(minBound, std::distance(mPairAccumulator.begin(), lower));
					mPairAppend.push_back(pair);
					pairAction(mPayloads[firstItem], mPayloads[secondItem]);
				}
			}
			if (mPairAppend.empty()) {
				continue;
			}
			uint32_t itemBound = mPairAppend.front().first;
			if (accumulatorSize == minBound) {
				if (!mPairAccumulator.empty() && mPairAccumulator.back().first < itemBound) {
					mPairAccumulator.clear();
				}
			}
			else {
				auto remover = [&](const auto& itPair) {
					return itPair.first < itemBound;
				};
				mPairAccumulator.erase(std::remove_if(mPairAccumulator.begin(), mPairAccumulator.begin() + minBound, remover), mPairAccumulator.begin() + minBound);
			}
			mPairAccumulator.insert(mPairAccumulator.end(), mPairAppend.begin(), mPairAppend.end());
			mPairAppend.clear();
			if (accumulatorSize != minBound)
			{
				std::sort(mPairAccumulator.begin(), mPairAccumulator.end(), PairCompare{});
			}
		}
	}

private:
	struct Area {
		int MinI;
		int MinJ;
		int MaxI;
		int MaxJ;
	};

	using IndexPair = std::pair<uint32_t, uint32_t>;
	struct PairCompare {
		bool operator () (const IndexPair& left, const IndexPair& right) {
			if (left.first == right.first) {
				return left.second < right.second;
			}
			return left.first < right.first;
		}
	};

	inline std::tuple<int, int> CellIntCoords(const Vector2& point) {
		float relativeX = point.x / Extents.x;
		float relativeY = point.y / Extents.y;
		assert(relativeX >= -1.f);
		assert(relativeY >= -1.f);
		return { static_cast<int>((1.f + relativeX) * CountX) - CountX, static_cast<int>((1.f + relativeY) * CountY) - CountY };
	}

	inline uint32_t GetCellID(const int i, const int j) {
		int iMod = (i % CountX + CountX) % CountX;
		int jMod = (j % CountY + CountY) % CountY;
		return iMod + jMod * CountX;
	}

	template<typename TAction>
	void IterateArea(const Area& area, TAction&& action) {
		assert(area.MinI < CountX&& area.MinJ < CountY);
		assert(area.MaxI >= 0 && area.MaxJ >= 0);
		for (int j = area.MinJ; j <= area.MaxJ; ++j) {
			for (int i = area.MinI; i <= area.MaxI; ++i) {
				action(GetCellID(i, j));
			}
		}
	}

	Vector2 Extents;
	int CountX;
	int CountY;

	std::vector<TPayload> mPayloads;
	std::vector<uint32_t> mPackedCells;
	std::vector<uint32_t> mSparseCells;
	std::vector<uint32_t> mCellCounts;
	std::vector<uint32_t> mCellFirst;
	std::vector<uint32_t> mPartition;
	std::vector<Area> mInsertionAreas;

	std::vector<IndexPair> mPairAccumulator;
	std::vector<IndexPair> mPairAppend;
};