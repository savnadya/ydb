#pragma once
#include "position.h"
#include "heap.h"
#include "result_builder.h"
#include "batch_iterator.h"

#include <ydb/core/formats/arrow/arrow_filter.h>

namespace NKikimr::NArrow::NMerger {

class TMergePartialStream {
private:
#ifndef NDEBUG
    std::optional<TSortableBatchPosition> CurrentKeyColumns;
#endif
    bool PossibleSameVersionFlag = true;

    std::shared_ptr<arrow::Schema> SortSchema;
    std::shared_ptr<arrow::Schema> DataSchema;
    const bool Reverse;
    const std::vector<std::string> VersionColumnNames;
    ui32 ControlPoints = 0;

    TSortingHeap<TBatchIterator> SortHeap;

    NJson::TJsonValue DebugJson() const {
        NJson::TJsonValue result = NJson::JSON_MAP;
#ifndef NDEBUG
        if (CurrentKeyColumns) {
            result["current"] = CurrentKeyColumns->DebugJson();
        }
#endif
        result.InsertValue("heap", SortHeap.DebugJson());
        return result;
    }

    std::optional<TSortableBatchPosition> DrainCurrentPosition();

    void CheckSequenceInDebug(const TSortableBatchPosition& nextKeyColumnsPosition);
public:
    TMergePartialStream(std::shared_ptr<arrow::Schema> sortSchema, std::shared_ptr<arrow::Schema> dataSchema, const bool reverse, const std::vector<std::string>& versionColumnNames)
        : SortSchema(sortSchema)
        , DataSchema(dataSchema)
        , Reverse(reverse)
        , VersionColumnNames(versionColumnNames)
    {
        Y_ABORT_UNLESS(SortSchema);
        Y_ABORT_UNLESS(SortSchema->num_fields());
        Y_ABORT_UNLESS(!DataSchema || DataSchema->num_fields());
    }

    void SkipToLowerBound(const TSortableBatchPosition& pos, const bool include) {
        if (SortHeap.Empty()) {
            return;
        }
        AFL_DEBUG(NKikimrServices::TX_COLUMNSHARD)("pos", pos.DebugJson().GetStringRobust())("heap", SortHeap.Current().GetKeyColumns().DebugJson().GetStringRobust());
        while (!SortHeap.Empty()) {
            const auto cmpResult = SortHeap.Current().GetKeyColumns().Compare(pos);
            if (cmpResult == std::partial_ordering::greater) {
                break;
            }
            if (cmpResult == std::partial_ordering::equivalent && include) {
                break;
            }
            const TSortableBatchPosition::TFoundPosition skipPos = SortHeap.MutableCurrent().SkipToLower(pos);
            AFL_DEBUG(NKikimrServices::TX_COLUMNSHARD)("pos", pos.DebugJson().GetStringRobust())("heap", SortHeap.Current().GetKeyColumns().DebugJson().GetStringRobust());
            if (skipPos.IsEqual()) {
                if (!include && !SortHeap.MutableCurrent().Next()) {
                    SortHeap.RemoveTop();
                } else {
                    SortHeap.UpdateTop();
                }
            } else if (skipPos.IsLess()) {
                SortHeap.RemoveTop();
            } else {
                SortHeap.UpdateTop();
            }
        }
    }

    void SetPossibleSameVersion(const bool value) {
        PossibleSameVersionFlag = value;
    }

    bool IsValid() const {
        return SortHeap.Size();
    }

    ui32 GetSourcesCount() const {
        return SortHeap.Size();
    }

    TString DebugString() const {
        return TStringBuilder() << "sort_heap=" << SortHeap.DebugJson();
    }

    void PutControlPoint(std::shared_ptr<TSortableBatchPosition> point);

    void RemoveControlPoint();

    bool ControlPointEnriched() const {
        return SortHeap.Size() && SortHeap.Current().IsControlPoint();
    }

    template <class TDataContainer>
    void AddSource(const std::shared_ptr<TDataContainer>& batch, const std::shared_ptr<NArrow::TColumnFilter>& filter) {
        if (!batch || !batch->num_rows()) {
            return;
        }
        if (filter && filter->IsTotalDenyFilter()) {
            return;
        }
//        Y_DEBUG_ABORT_UNLESS(NArrow::IsSorted(batch, SortSchema));
        auto filterImpl = (!filter || filter->IsTotalAllowFilter()) ? nullptr : filter;
        SortHeap.Push(TBatchIterator(batch, filterImpl, SortSchema->field_names(), DataSchema ? DataSchema->field_names() : std::vector<std::string>(), Reverse, VersionColumnNames));
    }

    bool IsEmpty() const {
        return !SortHeap.Size();
    }

    void DrainAll(TRecordBatchBuilder& builder);
    std::shared_ptr<arrow::Table> SingleSourceDrain(const TSortableBatchPosition& readTo, const bool includeFinish, std::optional<TSortableBatchPosition>* lastResultPosition = nullptr);
    bool DrainCurrentTo(TRecordBatchBuilder& builder, const TSortableBatchPosition& readTo, const bool includeFinish, std::optional<TSortableBatchPosition>* lastResultPosition = nullptr);
    bool DrainToControlPoint(TRecordBatchBuilder& builder, const bool includeFinish, std::optional<TSortableBatchPosition>* lastResultPosition = nullptr);
    std::vector<std::shared_ptr<arrow::RecordBatch>> DrainAllParts(const std::map<TSortableBatchPosition, bool>& positions,
        const std::vector<std::shared_ptr<arrow::Field>>& resultFields);
};

}
