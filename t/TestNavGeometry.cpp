/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

// Tier-2 navmesh routing regression suite. Each scenario under t/fixtures/nav/
// is a frozen routing problem — (map, start, goal) + expected outcome — replayed
// against a real sliced navmesh through DcNavHarness. Every historical geometry
// bug (ledge target unreachable, under-map seam, jump-gap island, route ending
// short) becomes one scenario line here, the geometry twin of the decision
// fixtures.
//
// Client-derived map data is NEVER committed. The slice lives under a gitignored
// DC_MAPDATA_DIR/mmaps (produced by tools/slice_mapdata.py). With no slice the
// whole suite GTEST_SKIPs, so a clean checkout still builds and runs Tier 1.
//
// Scenario format: one flat JSON object per line (shared DcDecisionJson), keys:
//   name (str), map (uint), sx,sy,sz, tx,ty,tz (float),
//   expectReachable (bool, default true),
//   expectComplete  (bool, optional — assert route reaches the goal poly),
//   maxStepZ        (float, optional — assert no vertical pop exceeds it),
//   minPoints       (uint,  optional — assert the polyline has >= this many pts).

#include "gtest/gtest.h"
#include "NavHarness.h"
#include "Ai/Dungeon/DungeonClear/Util/DcDecisionJson.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#ifndef DC_FIXTURE_DIR
#define DC_FIXTURE_DIR "."
#endif
#ifndef DC_MAPDATA_DIR
#define DC_MAPDATA_DIR "."
#endif

namespace
{
    namespace fs = std::filesystem;

    struct Scenario
    {
        std::string name;
        uint32_t    map = 0;
        float sx = 0, sy = 0, sz = 0, tx = 0, ty = 0, tz = 0;
        bool  expectReachable = true;
        bool  hasExpectComplete = false; bool expectComplete = false;
        bool  hasMaxStepZ = false;       float maxStepZ = 0.0f;
        bool  hasMinPoints = false;      uint32_t minPoints = 0;
    };

    std::vector<Scenario> LoadScenarios()
    {
        std::vector<Scenario> out;
        fs::path const dir = fs::path(DC_FIXTURE_DIR) / "nav";
        if (!fs::exists(dir))
            return out;
        for (auto const& entry : fs::directory_iterator(dir))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".json")
                continue;
            std::ifstream in(entry.path());
            std::string line;
            while (std::getline(in, line))
            {
                auto const parsed = DcDecisionJson::Parse(line);
                if (!parsed)
                    continue;  // blank / comment / not an object
                auto const& m = *parsed;
                Scenario s;
                s.name = DcDecisionJson::GetStr(m, "name", entry.path().stem().string());
                s.map = DcDecisionJson::GetU(m, "map", 0);
                s.sx = DcDecisionJson::GetF(m, "sx", 0);
                s.sy = DcDecisionJson::GetF(m, "sy", 0);
                s.sz = DcDecisionJson::GetF(m, "sz", 0);
                s.tx = DcDecisionJson::GetF(m, "tx", 0);
                s.ty = DcDecisionJson::GetF(m, "ty", 0);
                s.tz = DcDecisionJson::GetF(m, "tz", 0);
                s.expectReachable = DcDecisionJson::GetB(m, "expectReachable", true);
                if ((s.hasExpectComplete = DcDecisionJson::Has(m, "expectComplete")))
                    s.expectComplete = DcDecisionJson::GetB(m, "expectComplete", false);
                if ((s.hasMaxStepZ = DcDecisionJson::Has(m, "maxStepZ")))
                    s.maxStepZ = DcDecisionJson::GetF(m, "maxStepZ", 0);
                if ((s.hasMinPoints = DcDecisionJson::Has(m, "minPoints")))
                    s.minPoints = DcDecisionJson::GetU(m, "minPoints", 0);
                out.push_back(s);
            }
        }
        return out;
    }
}

TEST(DcNavGeometry, ScenariosRouteAsExpected)
{
    fs::path const mmapsDir = fs::path(DC_MAPDATA_DIR) / "mmaps";
    if (!fs::exists(mmapsDir))
        GTEST_SKIP() << "no sliced map data at " << mmapsDir
                     << " — run tools/slice_mapdata.py (never committed)";

    std::vector<Scenario> const scenarios = LoadScenarios();
    if (scenarios.empty())
        GTEST_SKIP() << "no nav scenarios under " << (fs::path(DC_FIXTURE_DIR) / "nav");

    std::map<uint32_t, std::shared_ptr<dtNavMesh>> meshes;  // per-map cache
    uint32_t ran = 0;
    uint32_t skipped = 0;

    for (Scenario const& s : scenarios)
    {
        auto it = meshes.find(s.map);
        if (it == meshes.end())
            it = meshes.emplace(s.map, DcNavHarness::LoadMap(DC_MAPDATA_DIR, s.map)).first;
        std::shared_ptr<dtNavMesh> const& mesh = it->second;
        if (!mesh)
        {
            ++skipped;  // map not sliced — covered by another run
            continue;
        }

        DcNavHarness::RouteResult const r =
            DcNavHarness::Route(mesh.get(), s.map, s.sx, s.sy, s.sz, s.tx, s.ty, s.tz);
        ASSERT_TRUE(r.built) << s.name;
        ++ran;

        EXPECT_EQ(r.reachable, s.expectReachable)
            << s.name << ": reachable mismatch (" << r.failureReason << ")";
        if (s.hasExpectComplete)
            EXPECT_EQ(r.corridorComplete, s.expectComplete) << s.name << ": completeness";
        if (s.expectReachable && s.hasMaxStepZ)
            EXPECT_LE(r.maxStepZ, s.maxStepZ)
                << s.name << ": vertical pop " << r.maxStepZ
                << " exceeds " << s.maxStepZ << " (under-map / ledge seam?)";
        if (s.expectReachable && s.hasMinPoints)
            EXPECT_GE(r.pointCount, s.minPoints) << s.name << ": too few route points";
    }

    if (ran == 0)
        GTEST_SKIP() << "nav scenarios present but no matching map slices ("
                     << skipped << " scenarios skipped)";
}

// ===========================================================================
// Pure geometry: FirstViolatedSphereOnPolyline (Phase B of the heroic
// over-pull transit plan). No navmesh needed — these run on every checkout.
// ===========================================================================

#include <cmath>

#include "Ai/Dungeon/DungeonClear/Util/DcEngageGeometry.h"

namespace
{
    using AvoidSphere = DcEngageGeometry::AvoidSphere;

    AvoidSphere Sphere(float x, float y, float r)
    {
        AvoidSphere s;
        s.x = x;
        s.y = y;
        s.r = r;
        return s;
    }

    // A polyline along +X at y=0 with `spacing` yards between points, starting
    // at the origin.
    std::vector<G3D::Vector3> StraightPolyline(size_t points, float spacing)
    {
        std::vector<G3D::Vector3> line;
        for (size_t i = 0; i < points; ++i)
            line.push_back(G3D::Vector3(spacing * float(i), 0.0f, 0.0f));
        return line;
    }
}

TEST(DcPolylineAvoidTest, PolylineClearOfSpheresReportsNoViolation)
{
    std::vector<G3D::Vector3> const line = StraightPolyline(8, 10.0f);
    std::vector<AvoidSphere> const spheres{ Sphere(35.0f, 20.0f, 4.0f),
                                            Sphere(-15.0f, 0.0f, 4.0f) };
    size_t leg = 999;
    EXPECT_EQ(DcEngageGeometry::FirstViolatedSphereOnPolyline(line, spheres, leg), -1);
}

// Ordering along the ROUTE, not distance from the walker, is the contract: a
// route that doubles back can violate a straight-line-nearer sphere on a LATER
// leg, and the one to stop at is the earlier one.
TEST(DcPolylineAvoidTest, PolylineViolationReportsFirstLegNotNearest)
{
    // P0(0,0) -> P1(10,0) -> P2(10,10) -> P3(-10,10): out, up, then back past
    // the walker.
    std::vector<G3D::Vector3> const line{
        G3D::Vector3(0.0f, 0.0f, 0.0f), G3D::Vector3(10.0f, 0.0f, 0.0f),
        G3D::Vector3(10.0f, 10.0f, 0.0f), G3D::Vector3(-10.0f, 10.0f, 0.0f) };
    // Sphere 0 sits on leg 2 and is NEARER the walker (11.2yd) than sphere 1
    // (11.7yd), which sits on leg 1.
    std::vector<AvoidSphere> const spheres{ Sphere(-5.0f, 10.0f, 3.0f),
                                            Sphere(10.0f, 6.0f, 3.0f) };
    size_t leg = 999;
    EXPECT_EQ(DcEngageGeometry::FirstViolatedSphereOnPolyline(line, spheres, leg), 1);
    EXPECT_EQ(leg, 1u);
}

// The reported leg START index is the truncation point: resizing the window to
// [0..legOut] leaves its last point OUTSIDE the sphere (the hazard threshold).
TEST(DcPolylineAvoidTest, WindowTruncatesBeforeSphereEntry)
{
    std::vector<G3D::Vector3> line = StraightPolyline(8, 10.0f);
    std::vector<AvoidSphere> const spheres{ Sphere(45.0f, 0.0f, 4.0f) };
    size_t leg = 999;
    int const idx = DcEngageGeometry::FirstViolatedSphereOnPolyline(line, spheres, leg);
    ASSERT_EQ(idx, 0);
    EXPECT_EQ(leg, 4u);

    line.resize(leg + 1);  // the caller's truncation
    ASSERT_EQ(line.size(), 5u);
    float const dx = line.back().x - spheres[0].x;
    float const dy = line.back().y - spheres[0].y;
    EXPECT_GT(std::sqrt(dx * dx + dy * dy), spheres[0].r);
}

// A sphere already covering the walker violates from the very first leg:
// legOut == 0, and the caller's truncation leaves <2 points — the "no window,
// fall through to normal handling" signal, never an empty vector or a freeze.
TEST(DcPolylineAvoidTest, TruncationLeavesAtLeastOneForwardPoint)
{
    std::vector<G3D::Vector3> line = StraightPolyline(5, 10.0f);
    std::vector<AvoidSphere> const spheres{ Sphere(0.0f, 0.0f, 5.0f) };
    size_t leg = 999;
    ASSERT_EQ(DcEngageGeometry::FirstViolatedSphereOnPolyline(line, spheres, leg), 0);
    EXPECT_EQ(leg, 0u);

    line.resize(leg + 1);
    EXPECT_EQ(line.size(), 1u);   // seed only -> caller reads "no spline window"
    EXPECT_FALSE(line.empty());   // but never empty
}

// A 2-point polyline replicates FirstViolatedSphere exactly, including the
// nearest-centre tie-break when several spheres violate the single leg.
TEST(DcPolylineAvoidTest, ExtractionPreservesSingleSegmentBehaviour)
{
    std::vector<G3D::Vector3> const line{ G3D::Vector3(0.0f, 0.0f, 0.0f),
                                          G3D::Vector3(30.0f, 0.0f, 0.0f) };
    std::vector<AvoidSphere> const spheres{ Sphere(20.0f, 0.0f, 4.0f),
                                            Sphere(10.0f, 0.0f, 4.0f) };

    int const single = DcEngageGeometry::FirstViolatedSphere(0.0f, 0.0f,
                                                             30.0f, 0.0f, spheres);
    size_t leg = 999;
    int const poly = DcEngageGeometry::FirstViolatedSphereOnPolyline(line, spheres, leg);
    EXPECT_EQ(single, 1);  // centre nearest the walker
    EXPECT_EQ(poly, single);
    EXPECT_EQ(leg, 0u);
}

// ===========================================================================
// Pure geometry: AggroReach — the single-source aggro-reach formula (Phase C
// of the heroic over-pull transit plan).
// ===========================================================================

#include "Ai/Dungeon/DungeonClear/Util/DungeonClearTuning.h"

// The invariant the phase exists to establish: the detection band
// (AggroRangeOf, buffer 0) and the avoidance sphere (BystanderSpheres, buffer
// PullEnRouteMargin) differ by exactly the party buffer and nothing else.
TEST(DcAggroReachTest, AggroReachDetectionAndAvoidanceAgreeUpToPartyBuffer)
{
    float const detection =
        DcEngageGeometry::AggroReachYards(24.0f, 5.0f, 1.5f, 2.0f, 0.0f);
    float const avoidance =
        DcEngageGeometry::AggroReachYards(24.0f, 5.0f, 1.5f, 2.0f, 4.0f);
    EXPECT_FLOAT_EQ(avoidance - detection, 4.0f);
}

// Pins the C.1 widening explicitly: detection includes the bot's combat reach
// and AggroRangeMargin (the pre-unification band was aggroRange + mobReach
// only), so the normal-difficulty behaviour change is a deliberate, visible
// decision rather than a silent one.
TEST(DcAggroReachTest, AggroReachIncludesBotCombatReachAndMargin)
{
    EXPECT_FLOAT_EQ(DcEngageGeometry::AggroReachYards(24.0f, 5.0f, 1.5f, 2.0f, 0.0f),
                    32.5f);
    // Zeroing the bot reach and the margin recovers the old band.
    EXPECT_FLOAT_EQ(DcEngageGeometry::AggroReachYards(24.0f, 5.0f, 0.0f, 0.0f, 0.0f),
                    29.0f);
}

// Phase C must not widen the ALONG-ROUTE reach — a pack 60yd along the route
// stays out of scope. That axis belongs to Phase A's window cap; raising both
// at once makes the live signal unattributable.
TEST(DcAggroReachTest, TrashBandRespectsLookahead)
{
    EXPECT_FLOAT_EQ(DC_CORRIDOR_LOOKAHEAD, 35.0f);
}
