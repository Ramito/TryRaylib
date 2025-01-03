#pragma once

#include <algorithm>
#include <assert.h>
#include <vector>

#include "raymath.h"

template <typename TPayload>
class SpatialPartition
{
public:
    void InitArea(Vector2 extents, int countX, int countY)
    {
        Extents = extents;
        CountX = countX;
        CountY = countY;
        mSparseCells.resize(CountX * CountY);
    }

    void Clear()
    {
        mPayloads.clear();
        mPackedCells.clear();
        //-------mSparseCells stays put
        mCellCounts.clear();
        mCellLookup.clear();
        mPartition.clear();
        mInsertionAreas.clear();
    }

    void InsertDeferred(TPayload payload, const Vector2& min, const Vector2& max)
    {
        assert(min.x <= max.x);
        assert(min.y <= max.y);

        mPayloads.push_back(payload);

        mInsertionAreas.push_back(ComputeArea(min, max));
    }

    void FlushInsertions()
    {
        auto countAction = [this](uint32_t cellIt) {
            uint32_t cellIndex = mSparseCells[cellIt];
            bool exists = cellIndex < mPackedCells.size() && mPackedCells[cellIndex] == cellIt;
            if (!exists) {
                cellIndex = mPackedCells.size();
                mSparseCells[cellIt] = cellIndex;
                mPackedCells.push_back(cellIt);
                mCellCounts.push_back(1);
            } else {
                mCellCounts[cellIndex]++;
            }
            return false;
        };
        for (const Area& area : mInsertionAreas) {
            IterateArea(area, countAction);
        }

        // Determine first index for each cell
        mCellLookup.resize(mCellCounts.size());
        uint32_t firstAcc = 0;
        auto firstIt = mCellLookup.begin();
        for (const uint32_t count : mCellCounts) {
            *firstIt = {firstAcc, count};
            ++firstIt;
            firstAcc += count;
        }

        mPartition.resize(firstAcc);

        uint32_t payloadId = 0;
        for (const Area& area : mInsertionAreas) {
            auto insertAction = [this, payloadId](uint32_t cellIt) {
                const uint32_t cellIndex = mSparseCells[cellIt];
                const uint32_t insertIndex =
                mCellLookup[cellIndex].First + mCellLookup[cellIndex].Count - mCellCounts[cellIndex];
                mCellCounts[cellIndex] -= 1;
                mPartition[insertIndex] = payloadId;
                return false;
            };
            IterateArea(area, insertAction);
            ++payloadId;
        }
    }

    template <typename TPairAction>
    void IteratePairs(TPairAction&& pairAction)
    {
        mPairAccumulator.clear();
        mPairAppend.clear();
        for (size_t cellIndex = 0; cellIndex < mPackedCells.size(); ++cellIndex) {
            const CellLookup cellLookup = mCellLookup[cellIndex];
            const size_t accumulatorSize = mPairAccumulator.size();
            size_t minBound = accumulatorSize;
            for (size_t firstIt = 0; firstIt < cellLookup.Count - 1; ++firstIt) {
                const uint32_t firstItem = mPartition[cellLookup.First + firstIt];
                for (size_t secondIt = firstIt + 1; secondIt < cellLookup.Count; ++secondIt) {
                    const uint32_t secondItem = mPartition[cellLookup.First + secondIt];
                    IndexPair pair = {firstItem, secondItem};
                    auto lower = std::lower_bound(mPairAccumulator.begin(), mPairAccumulator.begin() + accumulatorSize,
                                                  pair, PairCompare{});
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
            } else {
                auto remover = [&](const auto& itPair) { return itPair.first < itemBound; };
                mPairAccumulator.erase(std::remove_if(mPairAccumulator.begin(),
                                                      mPairAccumulator.begin() + minBound, remover),
                                       mPairAccumulator.begin() + minBound);
            }
            mPairAccumulator.insert(mPairAccumulator.end(), mPairAppend.begin(), mPairAppend.end());
            mPairAppend.clear();
            if (accumulatorSize != minBound) {
                std::sort(mPairAccumulator.begin(), mPairAccumulator.end(), PairCompare{});
            }
        }
    }

    template <typename TNearAction>
    void IterateNearby(const Vector2& min, const Vector2& max, TNearAction&& nearAction)
    {
        mNearbyPacked.clear();
        mNearbySparse.resize(mPayloads.size());

        const Area area = ComputeArea(min, max);

        auto areaCellIteration = [&](uint32_t cellID) {
            uint32_t cellIndex = mSparseCells[cellID];
            bool cellExists = cellIndex < mPackedCells.size() && mPackedCells[cellIndex] == cellID;
            if (!cellExists) {
                return false;
            }
            const CellLookup lookup = mCellLookup[cellIndex];
            for (uint32_t it = lookup.First; it < lookup.First + lookup.Count; ++it) {
                const uint32_t itemIndex = mPartition[it];
                const uint32_t packedIndex = mNearbySparse[itemIndex];
                bool alreadyIterated =
                packedIndex < mNearbyPacked.size() && mNearbyPacked[packedIndex] == itemIndex;
                if (alreadyIterated) {
                    continue;
                }
                if (nearAction(mPayloads[itemIndex])) {
                    return true;
                }
                mNearbySparse[itemIndex] = mNearbyPacked.size();
                mNearbyPacked.push_back(itemIndex);
            }
            return false;
        };

        IterateArea(area, areaCellIteration);
    }

private:
    struct Area
    {
        int MinI;
        int MinJ;
        int MaxI;
        int MaxJ;
    };

    inline Area ComputeArea(const Vector2& min, const Vector2& max)
    {
        auto [minI, minJ] = CellIntCoords(min);
        auto [maxI, maxJ] = CellIntCoords(max);

        return {minI, minJ, maxI, maxJ};
    }

    using IndexPair = std::pair<uint32_t, uint32_t>;
    struct PairCompare
    {
        bool operator()(const IndexPair& left, const IndexPair& right)
        {
            if (left.first == right.first) {
                return left.second < right.second;
            }
            return left.first < right.first;
        }
    };

    inline std::tuple<int, int> CellIntCoords(const Vector2& point)
    {
        float relativeX = point.x / Extents.x;
        float relativeY = point.y / Extents.y;
        assert(relativeX >= -1.f);
        assert(relativeY >= -1.f);
        return {static_cast<int>((1.f + relativeX) * CountX) - CountX,
                static_cast<int>((1.f + relativeY) * CountY) - CountY};
    }

    inline uint32_t GetCellID(const int i, const int j)
    {
        int iMod = (i % CountX + CountX) % CountX;
        int jMod = (j % CountY + CountY) % CountY;
        return iMod + jMod * CountX;
    }

    template <typename TAction>
    void IterateArea(const Area& area, TAction&& action)
    {
        assert(area.MinI <= CountX && area.MinJ <= CountY);
        assert(area.MaxI >= 0 && area.MaxJ >= 0);
        for (int j = area.MinJ; j <= area.MaxJ; ++j) {
            for (int i = area.MinI; i <= area.MaxI; ++i) {
                if (action(GetCellID(i, j))) {
                    return;
                }
            }
        }
    }

    Vector2 Extents;
    int CountX;
    int CountY;

    struct CellLookup
    {
        uint32_t First;
        uint32_t Count;
    };

    std::vector<TPayload> mPayloads;
    std::vector<uint32_t> mPackedCells;
    std::vector<uint32_t> mSparseCells;
    std::vector<uint32_t> mCellCounts; // Use only for baking
    std::vector<CellLookup> mCellLookup;
    std::vector<uint32_t> mPartition;
    std::vector<Area> mInsertionAreas;

    std::vector<IndexPair> mPairAccumulator;
    std::vector<IndexPair> mPairAppend;

    std::vector<uint32_t> mNearbyPacked;
    std::vector<uint32_t> mNearbySparse;
};
