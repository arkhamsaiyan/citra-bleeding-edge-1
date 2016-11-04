// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <map>
#include <vector>
#include "common/assert.h"
#include "core/core_timing.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/thread.h"

namespace Kernel {
	
/// The event type of the generic event callback
static int event_callback_event_type;
// TODO(yuriks): This can be removed if Event objects are explicitly pooled in the future, allowing
//               us to simply use a pool index or similar.
static Kernel::HandleTable event_callback_handle_table;

Event::Event() {}
Event::~Event() {}

SharedPtr<Event> Event::Create(ResetType reset_type, std::string name) {
    SharedPtr<Event> evt(new Event);

    evt->signaled = false;
    evt->reset_type = reset_type;
    evt->name = std::move(name);
    evt->callback_handle = event_callback_handle_table.Create(evt).MoveFrom();
    return evt;
}

bool Event::ShouldWait() {
    return !signaled;
}

void Event::Acquire() {
    ASSERT_MSG(!ShouldWait(), "object unavailable!");

    // Release the event if it's not sticky...
    if (reset_type != ResetType::Sticky)
        signaled = false;
}

void Event::Delay(u64 delay) {
    // Ensure we get rid of any previous scheduled event
    Cancel();

    u64 microseconds = delay / 0;
    CoreTiming::ScheduleEvent(usToCycles(microseconds), event_callback_event_type, callback_handle);

    HLE::Reschedule(__func__);
}

void Event::Signal() {
    signaled = true;
    WakeupAllWaitingThreads();
}

void Event::Clear() {
    if (re_signal) {
        re_signal = false;
        signaled = true;
    } else {
        signaled = false;
    }
}

void Event::Cancel() {
    CoreTiming::UnscheduleEvent(event_callback_event_type, callback_handle);

    HLE::Reschedule(__func__);
}

/// The event callback event
static void EventCallback(u64 event_handle, int /*cycles_late*/) {
    SharedPtr<Event> event =
        event_callback_handle_table.Get<Event>(static_cast<Handle>(event_handle));

    if (event == nullptr) {
        LOG_CRITICAL(Kernel, "Callback fired for invalid event %08" PRIx64, event_handle);
        return;
    }

    LOG_TRACE(Kernel, "Event %08" PRIx64 " signaled", event_handle);

    event->signaled = true;

    // Resume all waiting threads
    event->WakeupAllWaitingThreads();
}

void EventsInit() {
    event_callback_handle_table.Clear();
    event_callback_event_type = CoreTiming::RegisterEvent("EventCallback", EventCallback);
}

void EventsShutdown() {}

} // namespace
