/*
 * Copyright (c) 2026-present, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Assertions.h>
#include <AK/Math.h>
#include <AK/Platform.h>
#include <Compositor/VSyncScheduler.h>
#include <LibCore/Timer.h>

#if defined(AK_OS_MACOS)
#    include <AK/Atomic.h>
#    include <AK/AtomicRefCounted.h>
#    include <AK/NonnullRefPtr.h>
#    include <AK/RefPtr.h>
#    include <CoreGraphics/CoreGraphics.h>
#    include <CoreVideo/CoreVideo.h>
#    include <LibCore/EventLoop.h>
#endif

#if defined(AK_OS_WINDOWS)
#    include <AK/Atomic.h>
#    include <AK/AtomicRefCounted.h>
#    include <AK/NonnullRefPtr.h>
#    include <AK/RefPtr.h>
#    include <LibCore/EventLoop.h>

#    if !defined(WIN32_LEAN_AND_MEAN)
#        define WIN32_LEAN_AND_MEAN
#    endif
#    if !defined(NOMINMAX)
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif

namespace Compositor {

class TimerVSyncScheduler final : public VSyncScheduler {
public:
    AK_ALLOC_WITH_KMALLOC;

    explicit TimerVSyncScheduler(Function<void(MonotonicTime)>&& tick_callback)
        : m_tick_callback(move(tick_callback))
        , m_timer(Core::Timer::create_single_shot(0, [this] {
            fire();
        }))
    {
    }

    virtual ~TimerVSyncScheduler() override
    {
        m_timer->on_timeout = {};
        m_timer->stop();
    }

    virtual void schedule(double refresh_rate) override
    {
        VERIFY(refresh_rate == refresh_rate);
        VERIFY(refresh_rate > 0);
        VERIFY(refresh_rate < AK::Infinity<double>);

        auto refresh_rate_changed = m_refresh_rate != refresh_rate;
        m_refresh_rate = refresh_rate;
        auto now = MonotonicTime::now();
        if (!m_next_tick_time.has_value() || refresh_rate_changed)
            m_next_tick_time = now + frame_interval();
        else
            advance_next_tick_time_past(now);

        if (m_timer->is_active() && !refresh_rate_changed)
            return;

        arm_timer(now);
    }

    virtual Optional<MonotonicTime> most_recent_tick_time(MonotonicTime now, double refresh_rate) override
    {
        if (!m_next_tick_time.has_value() || m_refresh_rate != refresh_rate)
            return {};

        advance_next_tick_time_past(now);
        return *m_next_tick_time - frame_interval();
    }

private:
    void fire()
    {
        auto now = MonotonicTime::now();
        advance_next_tick_time_past(now);
        auto frame_time = *m_next_tick_time - frame_interval();
        m_tick_callback(frame_time);
    }

    AK::Duration frame_interval() const
    {
        return AK::Duration::from_nanoseconds(static_cast<i64>((1'000'000'000.0 / m_refresh_rate) + 0.5));
    }

    void advance_next_tick_time_past(MonotonicTime now)
    {
        VERIFY(m_next_tick_time.has_value());
        if (*m_next_tick_time > now)
            return;

        auto interval = frame_interval();
        auto intervals_to_advance = (now - *m_next_tick_time).to_nanoseconds() / interval.to_nanoseconds() + 1;
        *m_next_tick_time += AK::Duration::from_nanoseconds(intervals_to_advance * interval.to_nanoseconds());
    }

    void arm_timer(MonotonicTime now)
    {
        VERIFY(m_next_tick_time.has_value());
        advance_next_tick_time_past(now);
        VERIFY(*m_next_tick_time > now);

        // NB: The timer is armed against a fixed display grid. Rounding the delay up avoids firing before the grid
        //     point, and advancing by whole intervals keeps request and update latency from shifting later ticks.
        auto delay_nanoseconds = (*m_next_tick_time - now).to_nanoseconds();
        auto delay_milliseconds = (delay_nanoseconds + 999'999) / 1'000'000;
        m_timer->restart(static_cast<int>(max<i64>(delay_milliseconds, 1)));
    }

    Function<void(MonotonicTime)> m_tick_callback;
    double m_refresh_rate { 60.0 };
    RefPtr<Core::Timer> m_timer;
    Optional<MonotonicTime> m_next_tick_time;
};

#if defined(AK_OS_MACOS)

static constexpr int display_link_idle_stop_delay_ms = 250;

static CVReturn display_link_callback(CVDisplayLinkRef, CVTimeStamp const*, CVTimeStamp const*, CVOptionFlags, CVOptionFlags*, void* context);

class DisplayLinkState final : public AtomicRefCounted<DisplayLinkState> {
public:
    static RefPtr<DisplayLinkState> create(u64 display_id, Core::EventLoop& event_loop, Function<void(MonotonicTime)>&& tick_callback)
    {
        CVDisplayLinkRef display_link = nullptr;

#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wdeprecated-declarations"
        auto result = CVDisplayLinkCreateWithCGDisplay(static_cast<CGDirectDisplayID>(display_id), &display_link);
#    pragma clang diagnostic pop

        if (result != kCVReturnSuccess || !display_link)
            return nullptr;

        auto state = adopt_ref(*new DisplayLinkState(event_loop, display_link));

#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wdeprecated-declarations"
        result = CVDisplayLinkSetOutputCallback(display_link, display_link_callback, state.ptr());
#    pragma clang diagnostic pop

        if (result != kCVReturnSuccess)
            return nullptr;

        state->tick_callback = move(tick_callback);
        return state;
    }

    ~DisplayLinkState()
    {
        stop_display_link();
    }

    bool request_tick();
    void stop_display_link();
    void schedule_display_link_stop_if_idle();
    void stop_display_link_if_idle();
    bool consume_tick_request();
    bool is_valid() const;
    bool is_running() const;

    Core::EventLoop& event_loop;
    Function<void(MonotonicTime)> tick_callback;
    Atomic<bool> invalidated { false };
    Atomic<bool> tick_requested { false };
    Atomic<i64> most_recent_tick_time_nanoseconds { 0 };
    CVDisplayLinkRef display_link { nullptr };
    NonnullRefPtr<Core::Timer> idle_stop_timer;

private:
    DisplayLinkState(Core::EventLoop& event_loop, CVDisplayLinkRef display_link)
        : event_loop(event_loop)
        , display_link(display_link)
        , idle_stop_timer(Core::Timer::create_single_shot(display_link_idle_stop_delay_ms, [this] {
            stop_display_link_if_idle();
        }))
    {
    }
};

bool DisplayLinkState::request_tick()
{
    if (invalidated.load() || !display_link)
        return false;

    tick_requested.store(true);
    idle_stop_timer->stop();

#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wdeprecated-declarations"
    auto is_running = CVDisplayLinkIsRunning(display_link);
#    pragma clang diagnostic pop

    if (is_running)
        return true;

#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wdeprecated-declarations"
    auto result = CVDisplayLinkStart(display_link);
#    pragma clang diagnostic pop

    if (result == kCVReturnSuccess || result == kCVReturnDisplayLinkAlreadyRunning)
        return true;

    tick_requested.store(false);
    return false;
}

void DisplayLinkState::stop_display_link()
{
    invalidated.store(true);
    tick_requested.store(false);
    idle_stop_timer->on_timeout = {};
    idle_stop_timer->stop();

    if (!display_link)
        return;

#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wdeprecated-declarations"
    CVDisplayLinkStop(display_link);
#    pragma clang diagnostic pop
    CVDisplayLinkRelease(display_link);
    display_link = nullptr;
}

void DisplayLinkState::schedule_display_link_stop_if_idle()
{
    if (invalidated.load() || tick_requested.load() || !display_link)
        return;

    idle_stop_timer->restart(display_link_idle_stop_delay_ms);
}

void DisplayLinkState::stop_display_link_if_idle()
{
    if (invalidated.load() || tick_requested.load() || !display_link)
        return;

#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wdeprecated-declarations"
    CVDisplayLinkStop(display_link);
#    pragma clang diagnostic pop
}

bool DisplayLinkState::consume_tick_request()
{
    if (invalidated.load())
        return false;

    if (!tick_requested.exchange(false))
        return false;

    return true;
}

bool DisplayLinkState::is_valid() const
{
    return !invalidated.load();
}

bool DisplayLinkState::is_running() const
{
    if (invalidated.load() || !display_link)
        return false;

#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wdeprecated-declarations"
    auto running = CVDisplayLinkIsRunning(display_link);
#    pragma clang diagnostic pop
    return running;
}

class CVDisplayLinkVSyncScheduler final : public VSyncScheduler {
public:
    AK_ALLOC_WITH_KMALLOC;

    static OwnPtr<CVDisplayLinkVSyncScheduler> try_create(u64 display_id, Function<void(MonotonicTime)>&& tick_callback)
    {
        auto state = DisplayLinkState::create(display_id, Core::EventLoop::current(), move(tick_callback));
        if (!state)
            return nullptr;
        return adopt_own(*new CVDisplayLinkVSyncScheduler(state.release_nonnull()));
    }

    explicit CVDisplayLinkVSyncScheduler(NonnullRefPtr<DisplayLinkState> state)
        : m_state(move(state))
    {
    }

    virtual ~CVDisplayLinkVSyncScheduler() override
    {
        m_state->stop_display_link();
    }

    virtual void schedule(double) override
    {
        m_state->request_tick();
    }

    virtual Optional<MonotonicTime> most_recent_tick_time(MonotonicTime now, double refresh_rate) override
    {
        if (!m_state->is_running())
            return {};

        auto tick_time_nanoseconds = m_state->most_recent_tick_time_nanoseconds.load();
        if (tick_time_nanoseconds == 0 || tick_time_nanoseconds > now.nanoseconds())
            return {};

        auto age_nanoseconds = now.nanoseconds() - tick_time_nanoseconds;
        auto frame_interval_nanoseconds = static_cast<i64>((1'000'000'000.0 / refresh_rate) + 0.5);
        if (age_nanoseconds >= frame_interval_nanoseconds)
            return {};
        return now - AK::Duration::from_nanoseconds(age_nanoseconds);
    }

private:
    NonnullRefPtr<DisplayLinkState> m_state;
};

static CVReturn display_link_callback(CVDisplayLinkRef, CVTimeStamp const*, CVTimeStamp const*, CVOptionFlags, CVOptionFlags*, void* context)
{
    auto state = NonnullRefPtr { *static_cast<DisplayLinkState*>(context) };
    auto frame_time = MonotonicTime::now();
    state->most_recent_tick_time_nanoseconds.store(frame_time.nanoseconds());
    if (!state->consume_tick_request())
        return kCVReturnSuccess;

    // NB: Sample the shared monotonic clock on the display-link thread so deferred delivery does not shift the
    //     frame timestamp to the Compositor event-loop wakeup time.
    state->event_loop.deferred_invoke([state = move(state), frame_time] {
        if (!state->is_valid())
            return;
        state->tick_callback(frame_time);
        state->schedule_display_link_stop_if_idle();
    });
    return kCVReturnSuccess;
}

OwnPtr<VSyncScheduler> create_vsync_scheduler(Optional<u64> display_id, Function<void(MonotonicTime)>&& tick_callback)
{
    if (display_id.has_value()) {
        if (auto scheduler = CVDisplayLinkVSyncScheduler::try_create(*display_id, move(tick_callback)); scheduler)
            return scheduler;
    }

    return make<TimerVSyncScheduler>(move(tick_callback));
}

#elif defined(AK_OS_WINDOWS)

// NB: Windows has no CVDisplayLink equivalent that can be driven from an event loop, but the desktop
//     compositor exposes the display's vertical blank through DwmFlush(). Flushing on a dedicated
//     thread gives the phase-locked frame grid that macOS gets from CVDisplayLink, which the
//     timer-based fallback cannot reproduce: the default Windows timer quantum is ~15.6 ms, so a
//     60 Hz grid collapses onto a ~31 ms grid and the whole browser renders at half rate.

namespace {

static constexpr int dwm_flush_idle_stop_delay_ms = 250;
static constexpr int dwm_flush_frame_interval_ms = 16;

using DwmFlushProc = HRESULT(WINAPI*)();
using TimeBeginPeriodProc = UINT(WINAPI*)(UINT);
using TimeEndPeriodProc = UINT(WINAPI*)(UINT);

struct DisplayCompositionApi {
    DwmFlushProc flush { nullptr };
    TimeBeginPeriodProc begin_period { nullptr };
    TimeEndPeriodProc end_period { nullptr };
};

DisplayCompositionApi const& display_composition_api()
{
    // NB: The functions are resolved at runtime so that the compositor does not gain a hard link
    //     dependency on dwmapi and winmm.
    static auto api = [] {
        DisplayCompositionApi result;
        if (auto* module = LoadLibraryW(L"dwmapi.dll"))
            result.flush = reinterpret_cast<DwmFlushProc>(GetProcAddress(module, "DwmFlush"));
        if (auto* module = LoadLibraryW(L"winmm.dll")) {
            result.begin_period = reinterpret_cast<TimeBeginPeriodProc>(GetProcAddress(module, "timeBeginPeriod"));
            result.end_period = reinterpret_cast<TimeEndPeriodProc>(GetProcAddress(module, "timeEndPeriod"));
        }
        return result;
    }();
    return api;
}

class DwmFlushState final : public AtomicRefCounted<DwmFlushState> {
public:
    static RefPtr<DwmFlushState> create(Core::EventLoop& event_loop, Function<void(MonotonicTime)>&& tick_callback)
    {
        auto state = adopt_ref(*new DwmFlushState(event_loop));
        state->tick_callback = move(tick_callback);
        return state;
    }

    ~DwmFlushState()
    {
        stop_thread();
    }

    bool request_tick()
    {
        if (m_invalidated.load())
            return false;

        if (m_thread_running.load()) {
            m_tick_requested.store(true);
            return true;
        }

        // The previous thread may still be unwinding; wait for it so that two threads never drive
        // the same state.
        if (m_thread) {
            WaitForSingleObject(m_thread, 100);
            CloseHandle(m_thread);
            m_thread = nullptr;
        }

        m_tick_requested.store(true);
        m_thread_running.store(true);
        m_thread = CreateThread(nullptr, 0, &DwmFlushState::thread_proc, this, 0, nullptr);
        if (!m_thread) {
            m_thread_running.store(false);
            m_tick_requested.store(false);
            return false;
        }
        return true;
    }

    void stop_thread()
    {
        m_invalidated.store(true);
        m_tick_requested.store(false);
        if (!m_thread)
            return;

        WaitForSingleObject(m_thread, 200);
        CloseHandle(m_thread);
        m_thread = nullptr;
    }

    void run();

    bool is_valid() const { return !m_invalidated.load(); }
    bool is_running() const { return m_thread_running.load(); }
    i64 most_recent_tick_time_nanoseconds() const { return m_most_recent_tick_time_nanoseconds.load(); }

    Core::EventLoop& event_loop;
    Function<void(MonotonicTime)> tick_callback;

private:
    explicit DwmFlushState(Core::EventLoop& event_loop)
        : event_loop(event_loop)
    {
    }

    static DWORD WINAPI thread_proc(LPVOID context)
    {
        auto state = NonnullRefPtr { *static_cast<DwmFlushState*>(context) };
        state->run();
        state->m_thread_running.store(false);
        return 0;
    }

    Atomic<bool> m_invalidated { false };
    Atomic<bool> m_tick_requested { false };
    Atomic<bool> m_thread_running { false };
    Atomic<i64> m_most_recent_tick_time_nanoseconds { 0 };
    HANDLE m_thread { nullptr };
};

void DwmFlushState::run()
{
    auto const& api = display_composition_api();

    // Raise the timer resolution so that the pacing fallback below, and any LibCore timer that
    // cooperates with this thread, land on a millisecond grid instead of a 15.6 ms one.
    if (api.begin_period)
        api.begin_period(1);

    auto idle_deadline = MonotonicTime::now() + AK::Duration::from_milliseconds(dwm_flush_idle_stop_delay_ms);

    while (!m_invalidated.load()) {
        auto frame_time = MonotonicTime::now();
        auto flushed = false;
        if (api.flush && api.flush() == S_OK) {
            auto after_flush = MonotonicTime::now();
            // A real composition wait lasts most of a frame. Anything shorter means the compositor
            // is not running (DWM disabled, or a session without one), in which case DwmFlush()
            // returns immediately and this loop would otherwise spin.
            if (after_flush - frame_time >= AK::Duration::from_milliseconds(2)) {
                frame_time = after_flush;
                flushed = true;
            }
        }
        if (!flushed) {
            Sleep(dwm_flush_frame_interval_ms);
            frame_time = MonotonicTime::now();
        }

        m_most_recent_tick_time_nanoseconds.store(frame_time.nanoseconds());

        if (m_tick_requested.exchange(false)) {
            idle_deadline = MonotonicTime::now() + AK::Duration::from_milliseconds(dwm_flush_idle_stop_delay_ms);
            // NB: Sample the frame time on this thread so deferred delivery does not shift the
            //     frame timestamp to the Compositor event-loop wakeup time.
            event_loop.deferred_invoke([state = NonnullRefPtr { *this }, frame_time] {
                if (!state->is_valid())
                    return;
                state->tick_callback(frame_time);
            });
        }

        if (MonotonicTime::now() > idle_deadline)
            break;
    }

    if (api.end_period)
        api.end_period(1);
}

}

class DwmFlushVSyncScheduler final : public VSyncScheduler {
public:
    AK_ALLOC_WITH_KMALLOC;

    static OwnPtr<DwmFlushVSyncScheduler> try_create(Function<void(MonotonicTime)>&& tick_callback)
    {
        auto state = DwmFlushState::create(Core::EventLoop::current(), move(tick_callback));
        if (!state)
            return nullptr;
        return adopt_own(*new DwmFlushVSyncScheduler(state.release_nonnull()));
    }

    explicit DwmFlushVSyncScheduler(NonnullRefPtr<DwmFlushState> state)
        : m_state(move(state))
    {
    }

    virtual ~DwmFlushVSyncScheduler() override
    {
        m_state->stop_thread();
    }

    virtual void schedule(double) override
    {
        m_state->request_tick();
    }

    virtual Optional<MonotonicTime> most_recent_tick_time(MonotonicTime now, double refresh_rate) override
    {
        if (!m_state->is_running())
            return {};

        auto tick_time_nanoseconds = m_state->most_recent_tick_time_nanoseconds();
        if (tick_time_nanoseconds == 0 || tick_time_nanoseconds > now.nanoseconds())
            return {};

        auto age_nanoseconds = now.nanoseconds() - tick_time_nanoseconds;
        auto frame_interval_nanoseconds = static_cast<i64>((1'000'000'000.0 / refresh_rate) + 0.5);
        if (age_nanoseconds >= frame_interval_nanoseconds)
            return {};
        return now - AK::Duration::from_nanoseconds(age_nanoseconds);
    }

private:
    NonnullRefPtr<DwmFlushState> m_state;
};

OwnPtr<VSyncScheduler> create_vsync_scheduler(Optional<u64>, Function<void(MonotonicTime)>&& tick_callback)
{
    if (display_composition_api().flush) {
        if (auto scheduler = DwmFlushVSyncScheduler::try_create(move(tick_callback)); scheduler)
            return scheduler;
    }

    return make<TimerVSyncScheduler>(move(tick_callback));
}

#else

OwnPtr<VSyncScheduler> create_vsync_scheduler(Optional<u64>, Function<void(MonotonicTime)>&& tick_callback)
{
    return make<TimerVSyncScheduler>(move(tick_callback));
}

#endif

}
