// F107 checks (native on-foot VR): the game's camera away from the commander. Header only, run by native_frame_test (run107).
// The figures are the carrier flight's (log edvr_gfx_20261025_044101): the carrier management screen's camera 3367 m from the neck (+739, +174, +3280 m in the
// model), the standing eye 1.58 m, the upright neck 1.543 m, the eye height key 15 cm.
#pragma once

#include "../../src/d3d11/native_on_foot_core.h"
#include "../../src/d3d11/native_on_foot_f94.h"
#include "../../src/d3d11/native_on_foot_f95.h"
#include "../../src/d3d11/native_on_foot_f107.h"

#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>

namespace f107test {

namespace f94 = edvr::nofoot::f94;
namespace f95 = edvr::nofoot::f95;
namespace f107 = edvr::nofoot::f107;

template <class Check>
inline void run107(Check&& check) {
    const double neck[3] = {0.0, 1.543, 0.0};
    {   // the gate: the management screen's camera away, the commander's eye walking and in the largest second three flights had on
        const double screen[3] = {739.40, 1.543 + 174.25, 3280.48}, walking[3] = {0.023, 1.543 - 0.044, 0.173}, load[3] = {0.0, 1.543 - 0.94, 0.85};
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const double notANumber[3] = {nan, nan, nan};
        check(!f107::onCommander(f107::eyeNeckDistance(screen, neck)) && f107::onCommander(f107::eyeNeckDistance(walking, neck)) &&
                  f107::onCommander(f107::eyeNeckDistance(load, neck)) && !f107::onCommander(f107::eyeNeckDistance(notANumber, neck)),
              "F107: the carrier management screen's camera (3367 m from the neck) is away; the eye walking (0.18 m) and a load's first second (1.27 m) are on; not a number is away");
    }
    {   // the episode: on, then away for five calls (one not a number), then on for longer than the hold: one edge each way, its calls and distances (F208: the return waits the hold)
        f107::Away a;
        const double d[5] = {3367.4, 3367.6, std::numeric_limits<double>::quiet_NaN(), 139.2, 3367.5};
        int left = 0, back = 0;
        double backAt = -1.0;
        double t = 0.0;
        for (int i = 0; i < 2; ++i, t += 0.011) a.step(0.18, t);   // on the commander first: no episode
        const double awayAt = t;
        for (int i = 0; i < 5; ++i, t += 0.011) {
            const f107::Away::Edge e = a.step(d[i], t);
            left += e == f107::Away::kLeft;
            back += e == f107::Away::kBack;
        }
        const double nearAt = t;
        for (int i = 0; i < 250; ++i, t += 0.011) {   // 2.75 s of the camera on the commander
            const f107::Away::Edge e = a.step(0.19, t);
            left += e == f107::Away::kLeft;
            if (e == f107::Away::kBack) {
                ++back;
                backAt = t;
            }
        }
        check(left == 1 && back == 1 && a.episodes == 1 && a.farCalls == 5 && a.notNumber == 1 && std::fabs(a.nearestM - 139.2) < 1e-9 && std::fabs(a.farthestM - 3367.6) < 1e-9 && !a.away &&
                  a.alternations == 0 && backAt >= nearAt + f107::kReturnHoldS && backAt < nearAt + f107::kReturnHoldS + 0.012 && a.calls > 5 + 80 && awayAt < nearAt,
              "F107 (F208): one episode: left on the first call away, back on the first near call after the camera has stayed near for the 2.0 s hold, 5 far calls (1 not a number) and the near calls of the hold refused too, "
              "the camera 139.2 to 3367.6 m from the neck");
    }
    {   // F208: the suit shop's camera: a far view (the showroom, 710.5 m) and a near one (46 cm over the neck) alternating. At the call rate, and at 8 Hz with gaps up to the 0.47 s and 0.51 s the log had: one episode, never an edge
        f107::Away fast, slow;
        int leftFast = 0, backFast = 0, leftSlow = 0, backSlow = 0;
        double t = 0.0;
        for (int i = 0; i < 900; ++i, t += 1.0 / 90.0) {   // 10 s, every other call far, every other near
            const f107::Away::Edge e = fast.step(i % 2 == 0 ? 710.5 : 0.46, t);
            leftFast += e == f107::Away::kLeft;
            backFast += e == f107::Away::kBack;
        }
        t = 0.0;
        for (int i = 0; i < 900; ++i, t += 1.0 / 90.0) {   // 8 Hz: 60 ms far, 60 ms near; and two near runs of 0.47 s and 0.51 s
            const double sec = t;
            bool isFar = static_cast<int>(sec * 16.0) % 2 == 0;
            if (sec >= 3.0 && sec < 3.47) isFar = false;
            if (sec >= 6.0 && sec < 6.51) isFar = false;
            const f107::Away::Edge e = slow.step(isFar ? 710.5 : 0.46, t);
            leftSlow += e == f107::Away::kLeft;
            backSlow += e == f107::Away::kBack;
        }
        check(leftFast == 1 && backFast == 0 && fast.episodes == 1 && fast.away && fast.alternations > 400 && leftSlow == 1 && backSlow == 0 && slow.episodes == 1 && slow.away && slow.alternations > 40,
              "F208: a camera alternating between far and near, at the call rate for 10 s and at 8 Hz with near runs of 0.47 s and 0.51 s, is one episode: one edge, no end, never a line per flap");
        // and the return: the near camera for good ends it after the hold, once; the next far call is a new episode
        slow.step(710.5, t);   // the last far call, then the camera is near for good
        t += 1.0 / 90.0;
        const double nearFrom = t;
        int backs = 0;
        double backAt = -1.0;
        for (int i = 0; i < 270; ++i, t += 1.0 / 90.0) {
            const f107::Away::Edge e = slow.step(0.46, t);
            if (e == f107::Away::kBack) {
                ++backs;
                backAt = t;
            }
        }
        const bool again = slow.step(710.5, t) == f107::Away::kLeft && slow.episodes == 2;
        check(backs == 1 && backAt >= nearFrom + f107::kReturnHoldS - 1e-9 && backAt < nearFrom + f107::kReturnHoldS + 0.02 && again,
              "F208: a real return (the camera near for good) ends the episode once, after the 2.0 s hold; a later far call (within 3 s) is a new episode");
    }
    {   // F208b: the suit change's pattern (log edvr_gfx_20261101_141044, 14:13:03 to 14:13:29): far readings (3.6 to 3.7 m) at the recorded times, the camera near between them, 90 calls a second.
        // The 1.0 s hold made 18 episodes of it (each end let the pin, the neck reference and the settle take the near calls); the 2.0 s hold that doubles on a rejoin makes 3, whose accepted windows are
        // 0.73 s and 0.17 s; a steady 3.7 m camera (episodes 1 and 20 at 35.4 m) is one episode however long it stands
        static const double starts[18] = {3.144, 4.178, 5.711, 6.743, 9.503, 10.534, 11.613, 13.157, 14.190, 15.222, 16.246, 17.978, 19.523, 20.556, 21.589, 25.777, 26.809, 27.843};
        f107::Away a;
        int lefts = 0, backs = 0;
        double firstBackS = -1.0;
        uint32_t holdsSeen[4] = {};
        for (int i = 0; i < 40 * 90; ++i) {   // 40 s from 14:13:00 minus 8
            const double t = -8.0 + i / 90.0;
            bool isFar = t < 2.1 || (t >= 30.187 && t < 31.1);   // episodes 1 and 20: steady at 35.4 m
            for (double st : starts)
                if (t >= st - 0.012 && t < st + 0.005) isFar = true;
            const f107::Away::Edge e = a.step(isFar ? 3.7 : 0.46, t);
            lefts += e == f107::Away::kLeft;
            if (e == f107::Away::kBack) {
                ++backs;
                if (firstBackS < 0.0) firstBackS = t;
            }
            if (e == f107::Away::kLeft && a.episodes <= 4) holdsSeen[a.episodes - 1] = static_cast<uint32_t>(a.holdS);
        }
        check(a.episodes <= 4 && lefts <= 4 && backs <= 4 && holdsSeen[0] == 2 && holdsSeen[1] == 4,
              "F208b: the suit change's far readings 1 to 2.8 s apart (and a steady camera before and after) are 3 episodes, not 20: the 2.0 s hold covers the near stretches under it, a rejoin doubles it to 4.0 s");
        f107::Away steady;
        int steadyLefts = 0, steadyBacks = 0;
        for (int i = 0; i < 20 * 90; ++i) {   // 20 s steady at 3.7 m: one episode
            const f107::Away::Edge e = steady.step(3.7, i / 90.0);
            steadyLefts += e == f107::Away::kLeft;
            steadyBacks += e == f107::Away::kBack;
        }
        check(steadyLefts == 1 && steadyBacks == 0 && steady.episodes == 1 && steady.away && steady.alternations == 0 && steady.calls == 1800 && steady.farCalls == 1800,
              "F208b: a steady 3.7 m camera for 20 s stays one episode (the 1800 calls all refused, no end, no alternation)");
    }
    {   // F208b: the adaptive hold: a rejoin within kRejoinS of an end doubles it (up to kMaxHoldS), a quiet kRejoinS resets it, a silence starts clean
        f107::Away a;
        double t = 0.0;
        const auto run = [&](bool isFar, double secs) {
            const int n = static_cast<int>(secs * 90.0);
            for (int i = 0; i < n; ++i, t += 1.0 / 90.0) a.step(isFar ? 3.7 : 0.46, t);
        };
        run(true, 0.1);
        const double h0 = a.holdS;
        run(false, 2.5);               // near for 2.5 s: the 2.0 s hold ends it
        const bool ended1 = !a.away;
        run(true, 0.05);               // far again 0.5 s after the end: a rejoin
        const double h1 = a.holdS;
        run(false, 4.5);               // near for 4.5 s: the doubled 4.0 s hold ends it
        const bool ended2 = !a.away;
        run(true, 0.05);               // and again
        const double h2 = a.holdS;
        run(false, 9.0);               // 9 s near: the 8.0 s cap ends it
        const bool ended3 = !a.away;
        run(false, 3.5);               // quiet for more than kRejoinS
        run(true, 0.05);
        const double h3 = a.holdS;
        check(h0 == 2.0 && ended1 && h1 == 4.0 && ended2 && h2 == 8.0 && ended3 && h3 == 2.0 && a.episodes == 4 && a.rejoins == 0,
              "F208b: an episode that begins again within 3 s of an end doubles the hold (2.0, 4.0, 8.0 s, capped), and one that begins after 3 quiet seconds starts from 2.0 s again");
    }
    {   // F208: a stale report alone ends nothing: a gap in the hook's calls under the silence limit, the frame thread's reading of the report, and a silence past the limit
        f107::Away a;
        a.step(710.5, 0.0);
        a.step(710.5, 0.011);
        const f107::Away::Edge gap = a.step(710.5, 3.0);   // 3 s without a call (a menu, a pause): no edge, the same episode
        const f107::Away::Edge nearEdge = a.step(0.46, 3.011);
        const f107::Away::Edge farAgain = a.step(710.5, 3.5);
        check(gap == f107::Away::kNone && nearEdge == f107::Away::kNone && farAgain == f107::Away::kNone && a.away && a.episodes == 1 && a.alternations == 1,
              "F208: a gap of 3 s in the hook's calls ends nothing and starts nothing: the same episode");
        check(f107::awayNow(true, 1000, 1250) && f107::awayNow(true, 1000, 4000) && f107::awayNow(true, 1000, 6000) && !f107::awayNow(true, 1000, 6001) && !f107::awayNow(false, 1000, 1010) &&
                  !f107::awayNow(true, 1000, 900),
              "F208: the frame thread reads the hook's episode as away while it is published and its newest call is within 5 s (the old 250 ms ended it between a flapping camera's far calls)");
        f107::Away b;
        b.step(710.5, 0.0);
        const f107::Away::Edge silent = b.step(710.5, 6.0);   // past the 5 s of silence: the first episode is over (nothing was pinned) and this call starts the next
        check(silent == f107::Away::kLeft && b.episodes == 2 && b.away, "F208: a silence of 6 s ends the episode (nothing was pinned) and the next far call starts a new one");
    }
    {   // F208: nothing the away state takes out pumps: the neck reference's raise through the shop's alternating camera, against the old end rule. On the commander for 2 s (the raise +15.9 cm),
        // then the shop: calls alternate between the far camera (no target) and the near one (target -31.1 cm, the camera 46 cm over the neck).
        const auto run = [&](bool stableRule, double* minRaise, double* maxRise, double* tailRange, double* afterAway) {
            f94::NeckRaise nr;
            f107::Away a;
            f94::In in;
            in.dt = 1.0 / 90.0;
            in.settleS = 0.1;
            double prev = 0.0, tailMin = 1e9, tailMax = -1e9;
            *minRaise = 0.0;
            *maxRise = 0.0;
            for (int i = 0; i < 1620; ++i) {   // 18 s
                const double t = i / 90.0;
                const bool shop = t >= 2.0 && t < 12.0;
                const bool farCall = shop && i % 2 == 0;
                const double eyeNeck = farCall ? 710.5 : 0.46;
                bool away;
                if (stableRule) {
                    a.step(eyeNeck, t);
                    away = a.away;
                } else {
                    away = !f107::onCommander(eyeNeck);   // the old rule: the last call's own state
                }
                in.scope = !away;
                in.targetFresh = !away;
                in.targetM = shop ? -0.311 : 0.159;
                const f94::Out o = nr.step(in);
                if (t >= 3.0 && t < 12.0) {
                    *minRaise = std::fmin(*minRaise, o.raiseM);
                    if (t >= 3.0) {
                        tailMin = std::fmin(tailMin, o.raiseM);
                        tailMax = std::fmax(tailMax, o.raiseM);
                    }
                }
                if (t >= 2.0 && t < 12.0) *maxRise = std::fmax(*maxRise, o.raiseM - prev);
                prev = o.raiseM;
                if (i == 1619) *afterAway = o.raiseM;
            }
            *tailRange = tailMax - tailMin;
        };
        double minS = 0.0, riseS = 0.0, rangeS = 0.0, afterS = 0.0, minO = 0.0, riseO = 0.0, rangeO = 0.0, afterO = 0.0;
        run(true, &minS, &riseS, &rangeS, &afterS);
        run(false, &minO, &riseO, &rangeO, &afterO);
        check(std::fabs(minS) < 1e-9 && rangeS < 1e-9 && riseS <= 1e-9,
              "F208: through the shop's alternating camera the raise ramps out to exactly 0 within the ramp and stays there: it never goes below 0 and never rises again (the view is the game's own camera, nothing pumps)");
        check(minO < -0.25 && rangeO > 0.004 && riseO > 0.004 && afterO > 0.0,
              "F208 (control): with the old rule (each call's own state) the same camera pumps the raise down to about -29 cm: the view sits 29 cm low, and the ramp breathes with the calls");
        check(afterS > 0.10, "F208: and when the camera is back on the commander for good the raise comes back (+11 cm or more within 2 s of the return)");
    }
    {   // the neck reference through the screen: the settle takes no sample from a camera away and the raise ramps out (scope off), against the 1c26ea5f build
        const auto run = [&](bool gate, double* duringRaise, double* worstRaise, double* afterRaise) {
            f95::StanceSettle settle;
            f94::NeckRaise nr;
            f94::In in;
            in.dt = 1.0 / 90.0;
            in.settleS = 0.1;
            *worstRaise = 0.0;
            for (int i = 0; i < 720; ++i) {   // 8 s: on 0-2 s, the screen 2-5 s, on again 5-8 s
                const double t = i / 90.0;
                const bool screen = t >= 2.0 && t < 5.0;
                const double eye[3] = {screen ? 739.40 : 0.02, screen ? 1.543 + 174.25 : 1.58, screen ? 3280.48 : 0.17};
                const bool away = !f107::onCommander(f107::eyeNeckDistance(eye, neck));
                f95::SettleIn si;
                si.valid = !(gate && away);
                si.still = true;
                si.tS = t;
                si.neckY = neck[1];
                si.camY = eye[1];
                const f95::SettleOut so = settle.step(si);
                in.scope = !(gate && away);
                in.targetFresh = so.settled;
                if (so.settled) in.targetM = f94::raiseTarget(so.neckY, 0.15, so.camY);
                const f94::Out o = nr.step(in);
                *worstRaise = std::fmin(*worstRaise, o.raiseM);
                if (i == 440) *duringRaise = o.raiseM;   // 4.9 s, the screen
                if (i == 719) *afterRaise = o.raiseM;
            }
        };
        double during = 0.0, worst = 0.0, after = 0.0, duringOld = 0.0, worstOld = 0.0, afterOld = 0.0;
        run(true, &during, &worst, &after);
        run(false, &duringOld, &worstOld, &afterOld);
        check(std::fabs(during) < 0.0005 && worst > -0.0005 && std::fabs(after - (1.543 + 0.15 - 1.58)) < 0.0015,
              "F107: through the screen the raise ramps out to 0 (the view is the game's camera), never below it, and is back at the commander's +11.3 cm after");
        check(duringOld < -0.49 && worstOld < -0.49, "F107 (control): the 1c26ea5f build settles on the screen's camera and holds the raise at its -50 cm bound");
    }
    {   // the standing calibration: taken on the screen's camera, every later standing call reads 174 m under it and is binned crouched (the flight from 05:00:24)
        const double hScreen = 1.543 + 174.25, hStand = 1.58, eyeNow = 1.58;
        check(edvr::nofoot::pinCrouchWeight(true, hScreen, eyeNow) >= 1.0 && edvr::nofoot::pinCrouchWeight(true, hStand, eyeNow) == 0.0,
              "F107 (control): a standing calibration taken on the screen's camera makes every standing call crouched (w_c 1); the commander's own leaves it 0");
    }
    {   // Issue #175: an episode that ends and restarts within one frame must not lose its ended line or corrupt episode 1's counters.
        f107::Away a;
        f107::EventQueue q;
        f107::LogState log;
        struct Record {
            bool isLeft = false;
            uint32_t ep = 0, calls = 0, alternations = 0, rejoins = 0, shopCalls = 0, distCalls = 0;
            double durationS = 0.0, holdS = 0.0, maxM = 0.0;
        };
        std::vector<Record> records;
        auto stepPush = [&](double d, double tS, bool shop) {
            a.step(d, tS, shop);
            if (a.hasClosed) q.pushEnded(a.lastClosed);
            if (a.hasStarted) q.pushLeft(a.lastStarted);
        };
        auto tick = [&](uint64_t nowMs, bool timedOut = false) {
            f107::EpisodeEnd fallback;
            auto onLeft = [&](const f107::EpisodeStart& s) {
                records.push_back({true, s.episode, 0, 0, s.rejoins, 0, 0, 0.0, s.holdS, s.distanceM});
            };
            auto onEnded = [&](const f107::EpisodeEnd& e) {
                records.push_back({false, e.episode, e.calls, e.alternations, e.rejoins, e.shopCalls, e.distanceCalls,
                                   e.durationS, e.holdS, e.farthestM});
            };
            log.drainEvents(q, nowMs, onLeft, onEnded);
            if (timedOut) log.checkStale(nowMs, true, fallback, onEnded);
        };

        // Episode 1: away by distance + shop, one alternation, frame thread logs left(1).
        stepPush(3.7, 1.000, true);
        stepPush(0.46, 1.100, false);
        stepPush(4.2, 1.200, false);
        tick(1200);
        // Stay close for >= 2.0 s hold -> closes episode 1 at t=3.301 (avoiding 3.3 - 1.3 < 2.0 in IEEE-754), then rejoins 12 ms later at t=3.313 BEFORE the next frame tick.
        stepPush(0.46, 1.300, false);
        stepPush(0.46, 3.301, false);
        stepPush(3.8, 3.313, false);
        tick(3315);

        const bool subFrameOk =
            records.size() == 3 &&
            records[0].isLeft && records[0].ep == 1 &&
            !records[1].isLeft && records[1].ep == 1 && records[1].calls == 4 && records[1].alternations == 1 &&
            records[1].shopCalls == 1 && records[1].distCalls == 2 && std::fabs(records[1].durationS - 2.301) < 1e-6 &&
            std::fabs(records[1].maxM - 4.2) < 1e-6 &&
            records[2].isLeft && records[2].ep == 2 && records[2].rejoins == 1 && records[2].holdS == 4.0;
        check(subFrameOk,
              "F107 (#175): end + 12 ms rejoin within one frame emits ended(1) with episode 1's exact counters before left(2)");

        // Control: old polling-only logic (watching awayNow/episodes without the queue) sees away=true on both ticks and drops ended(1) permanently.
        {
            const bool oldWasAway = true;
            const bool oldNowAway = a.away;
            const bool oldEmitsEnded = oldWasAway && !oldNowAway;
            check(!oldEmitsEnded && a.episodes == 2,
                  "F107 (#175 control): polling awayNow alone misses ended(1) when episode 2 starts before the next tick");
        }

        // Same-call silence (>5 s) + immediate far restart:
        // Subcase A (frame thread stalled, no stale timeout emitted yet): emits ended(2) then left(3).
        records.clear();
        stepPush(5.1, 10.000, false);
        tick(10000);
        check(records.size() == 2 && !records[0].isLeft && records[0].ep == 2 && records[0].calls == 1 &&
                  records[1].isLeft && records[1].ep == 3 && records[1].holdS == 2.0,
              "F107 (#175): same-call silence close + immediate restart emits ended(2) then left(3) when frame thread was stalled");

        // Subcase B (frame thread ticked after stale timeout, closing ep 3 first): later silence+far must NOT double-emit ended(3).
        records.clear();
        f107::EpisodeEnd staleFb;
        staleFb.episode = 3;
        staleFb.calls = 1;
        staleFb.farthestM = 5.1;
        log.drainEvents(
            q, 16000,
            [&](const f107::EpisodeStart& s) {
                records.push_back({true, s.episode, 0, 0, s.rejoins, 0, 0, 0.0, s.holdS, s.distanceM});
            },
            [&](const f107::EpisodeEnd& e) {
                records.push_back({false, e.episode, e.calls, e.alternations, e.rejoins, e.shopCalls, e.distanceCalls,
                                   e.durationS, e.holdS, e.farthestM});
            });
        log.checkStale(
            16000, f107::awayStale(true, 10000, 16000), staleFb,
            [&](const f107::EpisodeEnd& e) {
                records.push_back({false, e.episode, e.calls, e.alternations, e.rejoins, e.shopCalls, e.distanceCalls,
                                   e.durationS, e.holdS, e.farthestM});
            });
        stepPush(6.0, 17.000, false);
        tick(17000);
        check(records.size() == 2 && !records[0].isLeft && records[0].ep == 3 &&
                  records[1].isLeft && records[1].ep == 4,
              "F107 (#175): stale-timeout close followed by silence+restart suppresses duplicate ended(3) and logs left(4)");

        // Subcase C (hook resumes right as stale-timeout fires, plus wait-free AwayStateBox & atomic SnapshotBox):
        // a mismatched episode N+1 snapshot must NEVER be relabeled as episode N, re-draining right before checkStale
        // lets episode N's exact kEnded win, and diagnostic snapshot contention must NEVER drop functional `away` to false.
        {
            f107::AwayStateBox awayBox;
            const uint64_t bigMs = (uint64_t{1} << 40) + 17000ull;
            awayBox.publish(true, bigMs);
            const f107::AwayState stOn = awayBox.load();
            awayBox.publish(false, 0);
            const f107::AwayState stOff = awayBox.load();
            check(stOn.away && stOn.lastMs == bigMs && !stOff.away && stOff.lastMs == 0,
                  "F107 (#175): AwayStateBox packs and loads away flag + 63-bit timestamp wait-free");

            f107::SnapshotBox box;
            f107::EpisodeEnd ep4Snap{};
            ep4Snap.episode = 4;
            ep4Snap.calls = 1;
            ep4Snap.farthestM = 6.0;
            box.publish(ep4Snap);
            awayBox.publish(true, 17000);
            f107::EpisodeEnd got{};
            check(box.read(&got) && got.episode == 4 && got.calls == 1 && std::fabs(got.farthestM - 6.0) < 1e-6,
                  "F107 (#175): atomic-word SnapshotBox publishes and reads a coherent episode-tagged snapshot");

            // Simulate writer paused mid-update (odd sequence): SnapshotBox::read returns false,
            // while AwayStateBox independently keeps functional away=true (neck offsets stay off).
            const uint32_t evenSeq = box.seqForTest();
            box.setSeqForTest(evenSeq | 1u);
            f107::EpisodeEnd contended{};
            const bool readWhileOdd = box.read(&contended);
            const f107::AwayState stWhileContended = awayBox.load();
            box.setSeqForTest(evenSeq);
            check(!readWhileOdd && f107::awayNow(stWhileContended.away, stWhileContended.lastMs, 17500),
                  "F107 (#175): contended SnapshotBox returns false without dropping wait-free functional away to false");

            // Simulate episode 4 open, silence > 5 s, hook wakes and queues exact ended(4) + left(5) and publishes ep 5.
            records.clear();
            stepPush(7.5, 24.000, false);   // closes ep 4 (1 call, 6.0 m) and starts ep 5 (1 call, 7.5 m)
            f107::EpisodeEnd ep5WrongFb{};
            ep5WrongFb.episode = 5;
            ep5WrongFb.calls = 99;
            ep5WrongFb.farthestM = 7.5;
            auto onLeftC = [&](const f107::EpisodeStart& s) {
                records.push_back({true, s.episode, 0, 0, s.rejoins, 0, 0, 0.0, s.holdS, s.distanceM});
            };
            auto onEndedC = [&](const f107::EpisodeEnd& e) {
                records.push_back({false, e.episode, e.calls, e.alternations, e.rejoins, e.shopCalls, e.distanceCalls,
                                   e.durationS, e.holdS, e.farthestM});
            };
            // Even without re-drain, a fallback tagged episode 5 must NOT close openEpisode 4.
            log.checkStale(24000, true, ep5WrongFb, onEndedC);
            check(records.empty() && log.logged && log.openEpisode == 4,
                  "F107 (#175): checkStale rejects a fallback snapshot tagged with episode N+1 instead of relabeling it as episode N");
            // Re-draining right before checkStale (as f107Tick does) lets exact ended(4) win over ep4Snap.
            log.drainEvents(q, 24000, onLeftC, onEndedC);
            log.checkStale(24000, true, ep4Snap, onEndedC);
            check(records.size() == 2 && !records[0].isLeft && records[0].ep == 4 && records[0].calls == 1 &&
                      std::fabs(records[0].maxM - 6.0) < 1e-6 && records[1].isLeft && records[1].ep == 5 &&
                      std::fabs(records[1].maxM - 7.5) < 1e-6,
                  "F107 (#175): pre-stale re-drain lets queued exact ended(4) win before left(5) without stale-fallback contamination");

            // Resumed same episode after >5 s gap: hook updates AwayState without pushing any edge;
            // revalidating AwayState after reading Snapshot + re-draining clears timedOut so episode 5 stays open
            // and the real kEnded is NOT dropped.
            records.clear();
            awayBox.publish(true, 24000);   // episode 5 initially appears stale at t=30000
            const f107::AwayState staleRead = awayBox.load();
            const bool initiallyTimedOut = f107::awayStale(staleRead.away, staleRead.lastMs, 30000);
            // Hook resumes episode 5 (still away, no edge pushed) and refreshes AwayState before Snapshot.
            awayBox.publish(true, 30000);
            f107::EpisodeEnd ep5Fresh{};
            ep5Fresh.episode = 5;
            ep5Fresh.calls = 2;
            box.publish(ep5Fresh);
            log.drainEvents(q, 30000, onLeftC, onEndedC);
            const f107::AwayState rechecked = awayBox.load();
            const bool stillTimedOut = f107::awayStale(rechecked.away, rechecked.lastMs, 30000);
            log.checkStale(30000, stillTimedOut, ep5Fresh, onEndedC);
            check(initiallyTimedOut && !stillTimedOut && records.empty() && log.logged && log.openEpisode == 5,
                  "F107 (#175): revalidating AwayState after snapshot+drain prevents false stale close when the same episode resumes");
        }

        // Race guards: awayStale must be FALSE on clean close (published=false) and on cross-thread tick skew (lastMs > nowMs),
        // and TRUE only when published=true and nowMs - lastMs > kAwayStaleMs.
        check(!f107::awayStale(false, 1000, 9000) && !f107::awayStale(true, 1001, 1000) &&
                  !f107::awayStale(true, 1000, 6000) && f107::awayStale(true, 1000, 6001) &&
                  !f107::awayStale(true, 0, 9000),
              "F107 (#175): awayStale fires only on genuine silence timeout, never on clean close or cross-thread clock skew");

        // Overflow: >kCapacity pushes without draining increments dropped() and keeps the first kCapacity events intact.
        {
            f107::EventQueue fullQ;
            uint32_t pushed = 0;
            for (uint32_t i = 1; i <= f107::EventQueue::kCapacity + 3; ++i) {
                f107::EpisodeStart s;
                s.episode = i;
                if (fullQ.pushLeft(s)) ++pushed;
            }
            f107::Event first{};
            const bool popped = fullQ.pop(&first);
            check(pushed == f107::EventQueue::kCapacity && fullQ.dropped() == 3 && popped && first.start.episode == 1,
                  "F107 (#175): EventQueue bounds capacity, counts overflow drops, and preserves FIFO order");
        }

        // Source pin (#175): hook publishes AwayState before Snapshot, and f107Tick reads Snapshot before
        // AwayState and revalidates AwayState after re-draining before committing checkStale.
        {
            std::ifstream f("src/d3d11/native_on_foot.cpp", std::ios::binary);
            std::stringstream ss;
            ss << f.rdbuf();
            const std::string cpp = ss.str();
            const size_t pubAway = cpp.find("g_f107AwayState.publish(g_f107Away.away");
            const size_t pubSnap = cpp.find("g_f107Snapshot.publish(curSnap);");
            const size_t recheck = cpp.find("const bool stillTimedOut = nofoot::f107::awayStale(awayState.away, awayState.lastMs, tickNow);");
            check(!cpp.empty() && pubAway != std::string::npos && pubSnap != std::string::npos && pubAway < pubSnap &&
                      recheck != std::string::npos,
                  "F107 (#175): hook publishes AwayState before Snapshot and f107Tick revalidates AwayState after snapshot+drain before checkStale");
        }
    }
}

}  // namespace f107test
