// F107 (native on-foot VR): the game's camera away from the commander. Header only, no game or D3D11 state, so native_frame_test runs the very functions the
// Animate post-hook uses. Journal: the arc doc, 2026-10-25, F107.
//
// On the pilot's fleet carrier (log edvr_gfx_20261025_044101, 1c26ea5f, Pimax Crystal Super on Pimax OpenXR) the carrier management screen showed the carrier
// from outside: from 04:59:57 to 05:00:23 the game's camera stood 3367 m from the commander's neck (eye - neck +739, +174, +3280 m in the model), while the neck
// read 1.541-1.545 m as before. Native on foot took that camera for the commander's eye: the body pin moved the body toward it (capped at 1.5 m), the neck
// reference's settle window found it still and published a raise of -174 m, clamped to its -50 cm bound (the view 50 cm under the game's camera: the screen's
// UI, placed at that camera, stood high), and after the pilot's two recentres (LOCAL generation 8 -> 10 at 05:00:02-04) the pin's standing calibration was taken
// again on it (h_stand 175.8 m), so from 05:00:24 to the session's end every call read 174 m under the standing eye and was binned crouching (w_c 1.00).
//
// The rule: the game's camera is on the commander while it is within kOnCommanderM of the animated neck. In three flights the largest eye - neck distance in
// any second with the camera on the commander was 1.27 m (a load's first second), else under 0.66 m. A call with the camera elsewhere (or not a number) is
// refused by the pin: the body keeps its animation, the neck reference and the standing calibration take nothing from it, and the frame thread ramps the
// neck reference's raise out (the view is the game's own camera, as for any camera the game moves away) and back in when the camera returns. F208: the return needs kReturnHoldS of a camera on
// the commander (below).
#pragma once

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace edvr {
namespace nofoot {
namespace f107 {

constexpr double kOnCommanderM = 3.0;

inline double eyeNeckDistance(const double eye[3], const double neck[3]) noexcept {
    const double d[3] = {eye[0] - neck[0], eye[1] - neck[1], eye[2] - neck[2]};
    return std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
}
inline bool onCommander(double eyeNeckM) noexcept { return eyeNeckM == eyeNeckM && eyeNeckM <= kOnCommanderM; }

// F208 (the suit shop at Pioneer Supplies, log edvr_gfx_20261101_114724, v0.22.0-rc.5): the game's showroom camera and a second camera at the commander (46 cm over his neck) took turns in the
// hook's reads every call, 45 times a second for 18 s. The episode ended on every near call and began on every far one (632 episodes, 1268 lines), so the neck reference's raise ramped in and
// out at the call rate: the view pumped, it settled near -29 cm (the near camera's target, ramped about half in) and the pilot saw it vibrate and sit low. The episode now ends only when the
// camera has stayed near for a hold: a camera that alternates between far and near is one episode, and a stale report alone ends nothing.
//
// F208b (the suit change in the same shop, log edvr_gfx_20261101_141044, v0.22.0-rc.5-1-g9b1b1f86, 14:13:03 to 14:13:29): the camera read 3.6 to 3.7 m from the neck on one call, then near for
// about a second, then far again: the 1.0 s hold ended 18 episodes (2 to 19), each beginning 12 ms to 1.4 s after the last ended, and every end let the pin, the neck reference (its target -31 cm
// from the camera 46 cm over the neck) and the settle take the near calls in between: the dip the pilot saw when he switched suits. The near stretches between the far reads were 1.0 to 2.8 s (and
// 4.2 s once, at the end). The hold is now 2.0 s, and it adapts: an episode that begins again within kRejoinS of an end is the same trouble, and the hold doubles (to kMaxHoldS) for it; one that
// begins after a quiet kRejoinS starts from the base again. The suit change's pattern then runs as 3 episodes in 40 s, not 20, and the first shop's flaps (near runs of 0.5 s) never end one.
//
// F209 (the game's own flag): the suit and the weapon buy screens at Pioneer Supplies are found by the open byte their interaction objects keep (shop_screen_core.h). While one is open the
// camera is held away at ANY distance (the weapon showroom's camera is the commander's own, 0.9 cm under the neck: no distance test could see it): `shopOpen` joins the distance test as a
// reason for a call to be refused, the episode's left line names the reason ("the shop screen", "the distance test" or both), and the episode ends through the same hold once the screen has closed
// and the camera is near the commander. The distance test is unchanged for everything else, and the adaptive hold stays: a fleet carrier's or a terminal's camera is not a buy screen.
//
// The hold. The first shop's near runs were at most 0.5 s and its real return 2.1 s; the suit change's near stretches 1.0 to 2.8 s. 2.0 s covers every flap of the first and most of the second, and
// the doubling covers the rest after one rejoin; a real return costs two seconds of the view being the game's own camera at the commander (the neck reference ramps back in over kRampS after it).
// Silence is another matter: with no call at all for kSilenceS the episode is over (the commander is not animated: a load, a different mode), and the next call starts clean.
constexpr double kReturnHoldS = 2.0;   // the base hold: the camera near for this long, no far read between, ends an episode
constexpr double kRejoinS = 3.0;       // a new episode within this long of an end doubles the hold
constexpr double kMaxHoldS = 8.0;      // ... up to this
constexpr double kSilenceS = 5.0;
constexpr uint64_t kAwayStaleMs = 5000;   // the frame thread's: an open episode is taken as over when the hook has not reported for this long (the hook reports on every call of an open episode, near ones too, so a flapping camera never ages)

// The frame thread's reading of the hook's published episode: away while the hook says so and its newest call is within kAwayStaleMs.
inline bool awayNow(bool published, uint64_t lastMs, uint64_t nowMs) noexcept { return published && nowMs >= lastMs && nowMs - lastMs <= kAwayStaleMs; }

// True only when an open published episode has genuinely gone silent past kAwayStaleMs.
// Distinguishes a real hook silence timeout from a clean close (published == false, whose
// kEnded snapshot is already queued in EventQueue) and from minor cross-thread clock skew
// where the Animate thread stamped lastMs slightly ahead of the frame thread's nowMs.
inline bool awayStale(bool published, uint64_t lastMs, uint64_t nowMs) noexcept {
    return published && lastMs != 0 && nowMs > lastMs && nowMs - lastMs > kAwayStaleMs;
}

// Immutable snapshots captured at each episode edge so a quick end + restart
// within one frame preserves episode N's exact ended counters and duration (#175).
struct EpisodeStart {
    uint32_t episode = 0;
    uint32_t rejoins = 0;
    double distanceM = 0.0;
    double holdS = kReturnHoldS;
    bool byShop = false;
    bool byDistance = false;
};

struct EpisodeEnd {
    uint32_t episode = 0;
    uint32_t calls = 0;
    uint32_t notNumber = 0;
    uint32_t alternations = 0;
    uint32_t rejoins = 0;
    uint32_t shopCalls = 0;
    uint32_t distanceCalls = 0;
    double durationS = 0.0;
    double holdS = kReturnHoldS;
    double nearestM = 0.0;
    double farthestM = 0.0;
};

// One episode of the camera away, for the edge lines: the calls the pin refused (every call from the first far one until the return), the far calls and the nearest and farthest distance of them
// (m; a camera that is not a number counts apart), the time and how often the camera came back near and left again before the hold ran out.
struct Away {
    bool away = false;
    uint32_t calls = 0, farCalls = 0, notNumber = 0, episodes = 0, alternations = 0, rejoins = 0;
    uint32_t shopCalls = 0, distanceCalls = 0;      // F209: this episode's calls refused with a buy screen open / with the camera over kOnCommanderM (one call can be both)
    bool beganByShop = false, beganByDistance = false;   // F209: what made the episode's first call away
    double startS = 0.0, lastS = 0.0, nearestM = 0.0, farthestM = 0.0, nearSinceS = -1.0, lastCallS = -1.0;
    double holdS = kReturnHoldS, endS = -1.0;   // the hold now in force (doubled by a rejoin) and when the last episode ended by it
    bool hasClosed = false;
    EpisodeEnd lastClosed{};
    bool hasStarted = false;
    EpisodeStart lastStarted{};
    enum Edge : uint8_t { kNone = 0, kLeft = 1, kBack = 2 };
    // One pinned call: its eye - neck distance and its time (s, monotonic). Returns the edge this call made. While an episode is open every call is refused (the pin takes nothing from a camera
    // that was away a moment ago), near ones too; the episode ends on the first near call after the camera has been near for holdS without a far one between.
    Edge step(double eyeNeckM, double tS, bool shopOpen = false) noexcept {
        hasClosed = false;
        hasStarted = false;
        Edge e = kNone;
        auto snapshotEnd = [&](double endTimeS) noexcept {
            lastClosed.episode = episodes;
            lastClosed.calls = calls;
            lastClosed.notNumber = notNumber;
            lastClosed.alternations = alternations;
            lastClosed.rejoins = rejoins;
            lastClosed.shopCalls = shopCalls;
            lastClosed.distanceCalls = distanceCalls;
            lastClosed.durationS = endTimeS >= startS ? (endTimeS - startS) : 0.0;
            lastClosed.holdS = holdS;
            lastClosed.nearestM = nearestM;
            lastClosed.farthestM = farthestM;
            hasClosed = true;
        };
        if (away && lastCallS >= 0.0 && tS - lastCallS > kSilenceS) {   // no call for kSilenceS: nothing was pinned, the episode is over
            snapshotEnd(lastS);
            away = false;
            nearSinceS = -1.0;
            endS = -1.0;
            e = kBack;
        }
        lastCallS = tS;
        const bool byDistance = !onCommander(eyeNeckM);
        const bool farNow = shopOpen || byDistance;   // F209: a buy screen open holds the camera away at any distance
        bool startedThisCall = false;
        if (farNow) {
            if (!away) {
                const bool rejoin = endS >= 0.0 && tS - endS <= kRejoinS;   // the camera went far again right after an end: the same trouble, the hold doubles
                holdS = rejoin ? (holdS * 2.0 > kMaxHoldS ? kMaxHoldS : holdS * 2.0) : kReturnHoldS;
                rejoins = rejoin ? rejoins + 1 : 0;
                away = true;
                ++episodes;
                calls = farCalls = notNumber = alternations = shopCalls = distanceCalls = 0;
                beganByShop = shopOpen;
                beganByDistance = byDistance;
                startS = tS;
                nearestM = farthestM = 0.0;
                nearSinceS = -1.0;
                startedThisCall = true;
                e = kLeft;
            } else if (nearSinceS >= 0.0) {   // back near for a while, and gone again before the hold: the same episode
                ++alternations;
                nearSinceS = -1.0;
            }
        } else if (away) {
            if (nearSinceS < 0.0) nearSinceS = tS;
            if (tS - nearSinceS >= holdS) {
                snapshotEnd(tS);
                away = false;
                nearSinceS = -1.0;
                endS = tS;
                e = kBack;
            }
        }
        if (away) {
            ++calls;
            lastS = tS;
            if (shopOpen) ++shopCalls;
            if (byDistance) ++distanceCalls;
            if (farNow) {
                ++farCalls;
                if (eyeNeckM == eyeNeckM) {
                    if (nearestM == 0.0 || eyeNeckM < nearestM) nearestM = eyeNeckM;
                    if (eyeNeckM > farthestM) farthestM = eyeNeckM;
                } else {
                    ++notNumber;
                }
            }
            if (startedThisCall) {
                lastStarted.episode = episodes;
                lastStarted.rejoins = rejoins;
                lastStarted.distanceM = farthestM;
                lastStarted.holdS = holdS;
                lastStarted.byShop = beganByShop;
                lastStarted.byDistance = beganByDistance;
                hasStarted = true;
            }
        }
        return e;
    }
};

// Lock-free SPSC edge queue from the Animate post-hook (producer) to the frame
// thread's f107Tick (consumer) so sub-frame end+restart transitions keep their
// ended lines and frozen counters (#175).
struct Event {
    enum Kind : uint8_t { kLeft = 0, kEnded = 1 };
    Kind kind = kLeft;
    EpisodeStart start{};
    EpisodeEnd end{};
};

class EventQueue {
  public:
    static constexpr uint32_t kCapacity = 16;

    bool pushLeft(const EpisodeStart& s) noexcept {
        Event ev;
        ev.kind = Event::kLeft;
        ev.start = s;
        return push(ev);
    }
    bool pushEnded(const EpisodeEnd& e) noexcept {
        Event ev;
        ev.kind = Event::kEnded;
        ev.end = e;
        return push(ev);
    }
    bool pop(Event* out) noexcept {
        const uint32_t tail = tail_.load(std::memory_order_relaxed);
        const uint32_t head = head_.load(std::memory_order_acquire);
        if (tail == head) return false;
        if (out) *out = ring_[tail % kCapacity];
        tail_.store(tail + 1, std::memory_order_release);
        return true;
    }
    uint32_t dropped() const noexcept { return dropped_.load(std::memory_order_relaxed); }

  private:
    bool push(const Event& ev) noexcept {
        const uint32_t head = head_.load(std::memory_order_relaxed);
        const uint32_t tail = tail_.load(std::memory_order_acquire);
        if (head - tail >= kCapacity) {
            dropped_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        ring_[head % kCapacity] = ev;
        head_.store(head + 1, std::memory_order_release);
        return true;
    }

    Event ring_[kCapacity]{};
    std::atomic<uint32_t> head_{0};
    std::atomic<uint32_t> tail_{0};
    std::atomic<uint32_t> dropped_{0};
};

// Wait-free functional camera-away state shared between the Animate post-hook
// (single writer) and f107Tick on the frame thread (single reader). Packed into
// one 64-bit atomic word ((lastMs << 1) | away) so the functional away decision
// is C++17 data-race-free and never drops to false under diagnostic snapshot
// contention (#175).
struct AwayState {
    bool away = false;
    uint64_t lastMs = 0;
};

class AwayStateBox {
  public:
    void publish(bool away, uint64_t lastMs) noexcept {
        constexpr uint64_t kMsMask = (uint64_t{1} << 63) - 1ull;
        const uint64_t packed = ((lastMs & kMsMask) << 1) | (away ? 1ull : 0ull);
        packed_.store(packed, std::memory_order_release);
    }

    AwayState load() const noexcept {
        const uint64_t packed = packed_.load(std::memory_order_acquire);
        return AwayState{(packed & 1ull) != 0, packed >> 1};
    }

  private:
    std::atomic<uint64_t> packed_{0};
};

// Coherent, C++17 data-race-free live episode snapshot published by the Animate
// post-hook under an atomic-word seqlock so f107Tick never pairs episode N's
// stale timeout with episode N+1's counters or performs non-atomic concurrent
// accesses (#175).
class SnapshotBox {
  public:
    static_assert(std::is_trivially_copyable<EpisodeEnd>::value, "EpisodeEnd must be trivially copyable");
    static constexpr size_t kWords = (sizeof(EpisodeEnd) + sizeof(uint64_t) - 1u) / sizeof(uint64_t);

    void publish(const EpisodeEnd& s) noexcept {
        uint64_t tmp[kWords]{};
        std::memcpy(tmp, &s, sizeof(s));
        const uint32_t seq = seq_.load(std::memory_order_relaxed);
        seq_.store(seq + 1u, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);
        for (size_t i = 0; i < kWords; ++i) {
            words_[i].store(tmp[i], std::memory_order_relaxed);
        }
        seq_.store(seq + 2u, std::memory_order_release);
    }

    bool read(EpisodeEnd* out) const noexcept {
        if (!out) return false;
        for (int attempt = 0; attempt < 4; ++attempt) {
            const uint32_t s1 = seq_.load(std::memory_order_acquire);
            if (s1 & 1u) continue;
            uint64_t tmp[kWords]{};
            for (size_t i = 0; i < kWords; ++i) {
                tmp[i] = words_[i].load(std::memory_order_relaxed);
            }
            std::atomic_thread_fence(std::memory_order_acquire);
            const uint32_t s2 = seq_.load(std::memory_order_relaxed);
            if (s1 == s2) {
                std::memcpy(out, tmp, sizeof(*out));
                return true;
            }
        }
        return false;
    }

    // Test hook: allows unit tests to verify read() behavior while a write is in progress.
    uint32_t seqForTest() const noexcept { return seq_.load(std::memory_order_relaxed); }
    void setSeqForTest(uint32_t s) noexcept { seq_.store(s, std::memory_order_relaxed); }

  private:
    std::atomic<uint32_t> seq_{0};
    std::atomic<uint64_t> words_[kWords]{};
};

// Frame-thread logger state: drains queued hook edges in FIFO order, suppresses
// a duplicate ended line if stale-timeout already closed that episode, and
// still closes an open episode on stale-timeout when hook calls stop.
struct LogState {
    bool logged = false;
    uint32_t openEpisode = 0;
    uint64_t leftMs = 0;

    template <typename OnLeftFn, typename OnEndedFn>
    void drainEvents(EventQueue& queue, uint64_t nowMs, OnLeftFn&& onLeft, OnEndedFn&& onEnded) noexcept {
        Event ev;
        while (queue.pop(&ev)) {
            if (ev.kind == Event::kLeft) {
                logged = true;
                openEpisode = ev.start.episode;
                leftMs = nowMs;
                onLeft(ev.start);
            } else if (ev.kind == Event::kEnded) {
                if (logged && openEpisode == ev.end.episode) {
                    logged = false;
                    onEnded(ev.end);
                }
            }
        }
    }

    template <typename OnEndedFn>
    void checkStale(uint64_t nowMs, bool timedOut, const EpisodeEnd& staleFallback, OnEndedFn&& onEnded) noexcept {
        if (timedOut && logged && staleFallback.episode == openEpisode) {
            logged = false;
            EpisodeEnd end = staleFallback;
            end.durationS = nowMs >= leftMs ? static_cast<double>(nowMs - leftMs) * 1e-3 : 0.0;
            onEnded(end);
        }
    }
};

}  // namespace f107
}  // namespace nofoot
}  // namespace edvr
