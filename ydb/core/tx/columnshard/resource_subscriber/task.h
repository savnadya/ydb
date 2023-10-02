#pragma once
#include <ydb/core/tx/columnshard/counters/common/object_counter.h>
#include "counters.h"

namespace NKikimr::NOlap::NResourceBroker::NSubscribe {

class ITask;

class TTaskContext {
private:
    YDB_READONLY_DEF(std::shared_ptr<TSubscriberTypeCounters>, Counters);
    YDB_READONLY_DEF(TString, TypeName);
public:
    TTaskContext(const TString& typeName, const std::shared_ptr<TSubscriberCounters>& subscriberCounters)
        : TypeName(typeName)
    {
        Counters = subscriberCounters->GetTypeCounters(TypeName);
    }
};

class TResourcesGuard: public NColumnShard::TMonitoringObjectsCounter<TResourcesGuard> {
private:
    const ui64 TaskId;
    const NActors::TActorId Sender;
    const ui64 Memory;
    const ui32 Cpu;
    const TTaskContext Context;
public:
    TResourcesGuard(const ui64 taskId, const ITask& task, const NActors::TActorId& sender, const TTaskContext& context);
    ~TResourcesGuard();
};

class ITask: public NColumnShard::TMonitoringObjectsCounter<ITask> {
private:
    YDB_READONLY(ui32, CPUAllocation, 0);
    YDB_READONLY(ui64, MemoryAllocation, 0);
    YDB_READONLY_DEF(TString, Name);
    YDB_READONLY_DEF(TString, Type);
    YDB_ACCESSOR(ui64, Priority, 0);
    TTaskContext Context;
protected:
    virtual void DoOnAllocationSuccess(const std::shared_ptr<TResourcesGuard>& guard) = 0;
public:
    ITask(const ui32 cpu, const ui64 memory, const TString& name, const TTaskContext& context)
        : CPUAllocation(cpu)
        , MemoryAllocation(memory)
        , Name(name)
        , Type(context.GetTypeName())
        , Context(context)
    {

    }

    const TTaskContext& GetContext() const {
        return Context;
    }

    TString DebugString() const {
        return TStringBuilder() << "cpu=" << CPUAllocation << ";mem=" << MemoryAllocation << ";name=" << Name << ";type=" << Type << ";priority=" << Priority << ";";
    }

    virtual ~ITask() = default;
    void OnAllocationSuccess(const ui64 taskId, const NActors::TActorId& senderId);

    static void Start(const NActors::TActorId& actorId, const std::shared_ptr<ITask>& task);
};

}