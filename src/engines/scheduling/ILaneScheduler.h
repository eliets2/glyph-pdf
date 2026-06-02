// SPDX-License-Identifier: Apache-2.0
// src/engines/scheduling/ILaneScheduler.h
#pragma once
#include <functional>
#include <QFuture>
#include <QString>

namespace gp {

enum class Lane { GPU, CPU, Any };
enum class TaskPriority { Low, Normal, High };

struct SchedulerOptions {
    Lane lane = Lane::CPU;
    TaskPriority priority = TaskPriority::Normal;
    int pageIndex = -1;
    QString taskName;
};

enum class SchedulerErrorCode {
    Cancelled,
    Timeout,
    LaneFull,
    WorkerCrashed,
    Unknown
};

struct SchedulerError {
    SchedulerErrorCode code = SchedulerErrorCode::Unknown;
    int pageIndex = -1;
    QString message;
};

// C++17-compatible result wrapper (std::expected requires C++23)
template<typename T>
struct ScheduledValue {
    T value{};
    SchedulerError error;
    bool ok = false;

    static ScheduledValue<T> success(T v) {
        ScheduledValue<T> r; r.value = std::move(v); r.ok = true; return r;
    }
    static ScheduledValue<T> failure(SchedulerError e) {
        ScheduledValue<T> r; r.error = std::move(e); r.ok = false; return r;
    }
};

template<typename T>
using SchedulerResult = QFuture<ScheduledValue<T>>;

class ILaneScheduler {
public:
    virtual ~ILaneScheduler() = default;

    // Submit a task to the specified lane. Returns a future that fires when
    // work completes or is cancelled. Never blocks the caller beyond the
    // QSemaphore acquire for the GPU lane.
    virtual SchedulerResult<int> submitInt(
        SchedulerOptions opts,
        std::function<int()> work) = 0;

    virtual void cancelAll() = 0;
    virtual int inFlightCount(Lane lane) const = 0;
    virtual void setLaneCapacity(Lane lane, int capacity) = 0;
};

} // namespace gp
