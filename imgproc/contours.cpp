/**
 * Copyright (c) 2017 Melown Technologies SE
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * *  Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef imgproc_contour_hpp_included_
#define imgproc_contour_hpp_included_

#include <bitset>
#include <map>
#include <list>
#include <algorithm>
#include <limits>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>

#include "dbglog/dbglog.hpp"

#include "utility/enum-io.hpp"

#include "./contours.hpp"

namespace bmi = boost::multi_index;

namespace imgproc {

namespace {

typedef detail::FindContourImpl::CellType CellType;

/** Image cell type
 */
enum : CellType {
    b0000 = 0x0, b0001 = 0x1, b0010 = 0x2, b0011 = 0x3
    , b0100 = 0x4, b0101 = 0x5, b0110 = 0x6, b0111 = 0x7
    , b1000 = 0x8, b1001 = 0x9, b1010 = 0xa, b1011 = 0xb
    , b1100 = 0xc, b1101 = 0xd, b1110 = 0xe, b1111 = 0xf
};

/** Segment orientation
 */
UTILITY_GENERATE_ENUM(Direction,
                      ((r))
                      ((l))
                      ((u))
                      ((d))
                      ((lu))
                      ((ld))
                      ((ru))
                      ((rd))
                      )

inline const char* arrow(Direction d)
{
    switch (d) {
    case Direction::r: return "\xe2\x86\x92";
    case Direction::l: return "\xe2\x86\x90";
    case Direction::u: return "\xe2\x86\x91";
    case Direction::d: return "\xe2\x86\x93";
    case Direction::lu: return "\xe2\x86\x96";
    case Direction::ld: return "\xe2\x86\x99";
    case Direction::ru: return "\xe2\x86\x97";
    case Direction::rd: return "\xe2\x86\x98";
    }
    return "?";
}

typedef math::Point2i Vertex;
typedef math::Points2i Vertices;

struct Segment {
    CellType type;
    Direction direction;
    Vertex start;
    Vertex end;

    mutable const Segment *prev;
    mutable const Segment *next;
    mutable const Segment *ringLeader;

    Segment(CellType type, Direction direction
            , Vertex start, Vertex end
            , const Segment *prev, const Segment *next)
        : type(type), direction(direction)
        , start(start), end(end)
        , prev(prev), next(next), ringLeader()
    {}
};

struct StartIdx {};
struct EndIdx {};

typedef boost::multi_index_container<
    Segment
    , bmi::indexed_by<
          bmi::ordered_unique<
              bmi::tag<StartIdx>
              , BOOST_MULTI_INDEX_MEMBER
              (Segment, decltype(Segment::start), start)
              >
          , bmi::ordered_unique<
                bmi::tag<EndIdx>
                , BOOST_MULTI_INDEX_MEMBER
                (Segment, decltype(Segment::end), end)
                >
          >
    > SegmentMap;

inline void distributeRingLeaderPrev(const Segment *s)
{
    // grab current ringLeader, move skip current and write ringLeader to all
    // previous segments
    const auto &ringLeader(s->ringLeader);
    for (s = s->prev; s; s = s->prev) { s->ringLeader = ringLeader; }
}

inline void distributeRingLeaderNext(const Segment *s)
{
    // grab current ringLeader, move skip current and write ringLeader to all
    // next segments
    const auto &ringLeader(s->ringLeader);
    for (s = s->next; s; s = s->next) { s->ringLeader = ringLeader; }
}

} // namespace

struct FindContour::Builder {
    Builder(FindContour &cf, const math::Size2 &rasterSize)
        : cf(cf), contour(rasterSize)
        , offset(cf.params_.pixelOrigin == PixelOrigin::center
                 ? math::Point2d() : math::Point2d(0.5, 0.5))
    {}

    template <typename Index>
    const Segment* find(Index &idx, const Vertex &v) {
        auto fidx(idx.find(v));
        return ((fidx == idx.end()) ? nullptr : &*fidx);
    }

    const Segment* findByStart(const Vertex &v) {
        return find(segments.get<StartIdx>(), v);
    }

    const Segment* findByEnd(const Vertex &v) {
        return find(segments.get<EndIdx>(), v);
    }

    void addSegment(CellType type, Direction direction, int i, int j
                    , const Vertex &start, const Vertex &end);

    void add(int i, int j, CellType type);

    void addBorder(int i, int j, CellType type);

    void addAmbiguous(CellType type, int i, int j);

    void extract(const Segment *head);

    void setBorder(CellType type, int i, int j);

    FindContour &cf;
    SegmentMap segments;
    Contour contour;
    math::Point2d offset;
};


void FindContour::Builder::setBorder(CellType type, int i, int j)
{
#define SET_BORDER(X, Y) contour.border.set(i + X, j + Y)

    switch (type) {
    case b0000: return;

    case b0001: return SET_BORDER(0, 1);
    case b0010: return SET_BORDER(1, 1);
    case b0100: return SET_BORDER(1, 0);
    case b1000: return SET_BORDER(0, 0);

    case b0011:
        SET_BORDER(0, 1);
        return SET_BORDER(1, 1);
    case b0110:
        SET_BORDER(1, 0);
        return SET_BORDER(1, 1);
    case b1100:
        SET_BORDER(0, 0);
        return SET_BORDER(1, 0);
    case b1001:
        SET_BORDER(0, 0);
        return SET_BORDER(0, 1);

    case b0101:
    case b0111:
    case b1010:
    case b1011:
    case b1101:
    case b1110:
        SET_BORDER(0, 0);
        SET_BORDER(1, 0);
        SET_BORDER(0, 1);
        return SET_BORDER(1, 1);

    case b1111: return;
    }

#undef SET_BORDER
}

void FindContour::Builder::addSegment(CellType type
                                      , Direction direction
                                      , int i, int j
                                      , const Vertex &start, const Vertex &end)
{
    setBorder(type, i, j);

    // mark in raster
    auto *prev(findByEnd(start));
    auto *next(findByStart(end));

    const auto &s
        (*segments.insert(Segment(type, direction, start, end
                                  , prev, next)).first);

    // LOG(info4) << "Segment " << s.start << " -> " << s.end << "> "
    //            << &s;

    // stranded segment -> we're done here
    if (!prev && !next) { return; }

    // insert new segment

    // link prev
    const Segment *pRingLeader(nullptr);
    if (prev) {
        prev->next = &s;
        pRingLeader = prev->ringLeader;
    }

    // link next
    const Segment *nRingLeader(nullptr);
    if (next) {
        next->prev = &s;
        nRingLeader = next->ringLeader;
    }

    // unify/create ringLeader
    if (!pRingLeader && !nRingLeader) {
        // no ringLeader, create for this segment
        s.ringLeader = &s;
        // and copy to others (there is at most one segment in each
        // direction -> simple assignment)
        if (next) { next->ringLeader = s.ringLeader; }
        if (prev) { prev->ringLeader = s.ringLeader; }
    } else if (!pRingLeader) {
        // distribute ringLeader from next node to all its previous segments
        distributeRingLeaderPrev(next);
    } else if (!nRingLeader) {
        // distribute ringLeader from prev node to all its next segments
        distributeRingLeaderNext(prev);
    } else if (pRingLeader != nRingLeader) {
        // both valid but different, prefer prev segment
        distributeRingLeaderNext(prev);
    } else {
        // both are the same -> we've just closed the ringLeader
        s.ringLeader = pRingLeader;

        // new ringLeader, extract contour
        extract(pRingLeader);
    }
}

#define ADD_SEGMENT(D, X1, Y1, X2, Y2)                                  \
    addSegment(type, Direction::D, i, j                                 \
               , { x + X1, y + Y1 }, { x + X2, y + Y2 })

void FindContour::Builder::addAmbiguous(CellType otype, int i, int j)
{
    // 2x scale to get rid of non-integral values
    auto x(i * 2);
    auto y(j * 2);

    Vertex v(x, y);

    auto type(cf.ambiguousType(v, otype));

    if (type == otype) {
        // same type
        switch (type) {
        case b0101: // b0111 + b1101
            ADD_SEGMENT(lu, 0, 1, 1, 0);
            return ADD_SEGMENT(rd, 2, 1, 1, 2);

        case b1010: // b1011 + b1110
            ADD_SEGMENT(ld, 1, 0, 2, 1);
            return ADD_SEGMENT(ru, 1, 2, 0, 1);
        }
    } else {
        // inverse type -> switch direction
        switch (type) {
        case b0101: // b1000 + b0010
            ADD_SEGMENT(ld, 1, 0, 0, 1);
            return ADD_SEGMENT(ru, 1, 2, 2, 1);

        case b1010: // b0100 + b0001
            ADD_SEGMENT(lu, 2, 1, 1, 0);
            return ADD_SEGMENT(rd, 0, 1, 1, 2);
        }
    }
}

void FindContour::Builder::add(int i, int j, CellType type)
{
    // 2x scale to get rid of non-integral values
    auto x(i * 2);
    auto y(j * 2);

    // LOG(info4) << "Adding inner: " << x << ", " << y;

    switch (type) {
    case b0000: return;
    case b0001: return ADD_SEGMENT(rd, 0, 1, 1, 2);
    case b0010: return ADD_SEGMENT(ru, 1, 2, 2, 1);
    case b0011: return ADD_SEGMENT(r, 0, 1, 2, 1);
    case b0100: return ADD_SEGMENT(lu, 2, 1, 1, 0);
    case b0101: return addAmbiguous(type, i, j);
    case b0110: return ADD_SEGMENT(u, 1, 2, 1, 0);
    case b0111: return ADD_SEGMENT(ru, 0, 1, 1, 0);
    case b1000: return ADD_SEGMENT(ld, 1, 0, 0, 1);
    case b1001: return ADD_SEGMENT(d, 1, 0, 1, 2);
    case b1010: return addAmbiguous(type, i, j);
    case b1011: return ADD_SEGMENT(rd, 1, 0, 2, 1);
    case b1100: return ADD_SEGMENT(l, 2, 1, 0, 1);
    case b1101: return ADD_SEGMENT(ld, 2, 1, 1, 2);
    case b1110: return ADD_SEGMENT(lu, 1, 2, 0, 1);
    case b1111: return;
    }
}

void FindContour::Builder::addBorder(int i, int j, CellType type)
{
    // 2x scale to get rid of non-integral values
    auto x(i * 2);
    auto y(j * 2);

    // LOG(info4) << "Adding border: " << x << ", " << y;

    switch (type) {
    case b0000: return;

    case b0001:
        ADD_SEGMENT(r, 0, 1, 1, 1);
        return ADD_SEGMENT(d, 1, 1, 1, 2);

    case b0010:
        ADD_SEGMENT(u, 1, 2, 1, 1);
        ADD_SEGMENT(r, 1, 1, 2, 1);

    case b0011: return ADD_SEGMENT(r, 0, 1, 2, 1);

    case b0100:
        ADD_SEGMENT(l, 2, 1, 1, 1);
        return ADD_SEGMENT(u, 1, 1, 1, 0);

    case b0101: // b0111 + b1101
        ADD_SEGMENT(u, 0, 1, 0, 0);
        ADD_SEGMENT(r, 0, 0, 1, 0);
        ADD_SEGMENT(d, 2, 1, 2, 2);
        return ADD_SEGMENT(l, 2, 2, 1, 2);

    case b0110: return ADD_SEGMENT(u, 1, 2, 1, 0);

    case b0111:
        ADD_SEGMENT(u, 0, 1, 0, 0);
        return ADD_SEGMENT(r, 0, 0, 1, 0);

    case b1000:
        ADD_SEGMENT(d, 1, 0, 1, 1);
        return ADD_SEGMENT(l, 1, 1, 0, 1);

    case b1001: return ADD_SEGMENT(d, 1, 0, 1, 2);

    case b1010: // b1011 + b1110
        ADD_SEGMENT(r, 1, 0, 2, 0);
        ADD_SEGMENT(d, 2, 0, 2, 1);
        ADD_SEGMENT(l, 1, 2, 0, 2);
        return ADD_SEGMENT(u, 0, 2, 0, 1);

    case b1011:
        ADD_SEGMENT(r, 1, 0, 2, 0);
        return ADD_SEGMENT(d, 2, 0, 2, 1);

    case b1100: return ADD_SEGMENT(l, 2, 1, 0, 1);

    case b1101:
        ADD_SEGMENT(d, 2, 1, 2, 2);
        return ADD_SEGMENT(l, 2, 2, 1, 2);

    case b1110:
        ADD_SEGMENT(l, 1, 2, 0, 2);
        return ADD_SEGMENT(u, 0, 2, 0, 1);

    case b1111: return;
    }
}

#undef ADD_SEGMENT

void FindContour::Builder::extract(const Segment *head)
{
    // LOG(info4) << "Processing ring from: " << head;
    contour.rings.emplace_back();
    auto &ring(contour.rings.back());

    const auto addVertex([&](const Vertex &v) {
            ring.emplace_back(v(0) / 2.0 + offset(0), v(1) / 2.0 + offset(1));
        });

    // add first vertex
    addVertex(head->start);

    // end segment
    const auto end((head->type != head->prev->type) ? head : head->prev);

    // process full ringLeader
    for (const auto *p(head), *s(head->next); s != end; p = s, s = s->next)
    {
        if (s->ringLeader != head) {
            LOGTHROW(info4, std::runtime_error)
                << "Segment: " << s << " doesn't belong to ring "
                << head << " but " << s->ringLeader << ".";
        }

        if (!s->next) {
            LOGTHROW(info4, std::runtime_error)
                << "Segment: type: [" << std::bitset<4>(s->type) << "/"
                << s->direction << "] "
                << "<" << s->start << " -> " << s->end << "> "
                << s << " in ringLeader "
                << s->ringLeader << " has no next segment.";
            }

#if 0
        LOG(info4) << "[" << std::bitset<4>(s->type) << "]: "
                   << arrow(s->direction) << " (" << s->direction
                   << "): " << (s->start / 2.0)
                   << " -> " << (s->end / 2.0);
#endif

        // add vertex only when direction differs
        if (!cf.params_.joinStraightSegments
            || (s->direction != p->direction))
        {
            addVertex(s->start);
        }
    }
}

Contour FindContour::operator()(const Contour::Raster &raster)
{
    const auto size(raster.dims());

    Builder cb(*this, size);

    const auto getFlags([&](int x, int y) -> CellType
    {
        return (raster.get(x, y + 1)
                | (raster.get(x + 1, y + 1) << 1)
                | (raster.get(x + 1, y) << 2)
                | (raster.get(x, y) << 3));
    });

#define ADD(x, y) cb.add(x, y, getFlags(x, y))
#define ADD_BORDER(x, y) cb.addBorder(x, y, getFlags(x, y))

    int xend(size.width - 1);
    int yend(size.height - 1);

    // first row
    for (int i(-1); i <= xend; ++i) { ADD_BORDER(i, -1); }

    // all inner rows
    for (int j(0); j < yend; ++j) {
        // first column
        ADD_BORDER(-1, j);

        // all inner columns
        for (int i(0); i < xend; ++i) { ADD(i, j); }

        // last column
        ADD_BORDER(xend, j);
    }

    // last row
    for (int i(-1); i <= xend; ++i) { ADD_BORDER(i, yend); }

#undef ADD
#undef ADD_BORDER

    return cb.contour;
}

namespace {

typedef std::set<math::Point2d> LockedPoints;

LockedPoints findLockedPoints(const Contour::list &contours)
{
    // TODO: lock border points as well

    // compute cardinality of points (number of rings it belongs to)
    std::map<math::Point2d, int> cardinality;
    for (const auto &contour : contours) {
        for (const auto &ring : contour.rings) {
            for (const auto &p : ring) {
                ++cardinality[p];
            }
        }
    }

    // extract points with cardinality > 2
    LockedPoints locked;
    for (const auto &item : cardinality) {
        if (item.second > 2) { locked.insert(item.first); }
    }
    return locked;
}

/** Compute area of parellelogram defined three points.
 *
 *  Thus we get 2x area of triangle defined by the same three points.
 */
template <typename T>
double area(const T &a, const T &b, const T &c)
{
    return std::abs(((b(0) - a(0)) * (c(1) - a(1)))
                    - ((c(0) - a(0)) * (b(1) - a(1))));
}

math::Polygon simplifyRing(const math::Polygon &ring
                           , const LockedPoints &lockedPoints
                           , double stopCondition)
{
    const auto InvalidArea(std::numeric_limits<double>::infinity());

    // double the stop condition becase we compute are of parallelograms
    stopCondition *= 2;

    // do nothing for too small rings
    if (ring.size() <= 4) { return ring; }

    typedef std::list<math::Point3> LinkedPoints;

    // work heap
    typedef std::vector<LinkedPoints::iterator> Heap;
    Heap heap;
    heap.reserve(ring.size());

    // construct work area
    LinkedPoints points;

    // add point to liked list + add to heap if not locked
    const auto &add([&](const math::Point2d &prev, const math::Point2d &p
                        , const math::Point2d &next)
    {
        if (lockedPoints.find(p) == lockedPoints.end()) {
            // unlocked
            points.emplace_back(p(0), p(1), area(prev, p, next));
            // not locked -> remember iterator
            heap.push_back(std::prev(points.end()));
        } else {
            // locked
            points.emplace_back(p(0), p(1), InvalidArea);
        }
    });

    // add first point
    add(ring.back(), ring.front(), ring[1]);

    // process inner points
    for (std::size_t i(1), e(ring.size() - 1); i != e; ++i) {
        add(ring[i - 1], ring[i], ring[i + 1]);
    }

    // add last point
    add(ring[ring.size() - 2], ring.back(), ring.front());

    // NB: make_heap create max-heap => we need to invert order
    const auto compare([&](const Heap::value_type &l
                           , const Heap::value_type &r) -> bool
    {
        const auto &p1(*l);
        const auto &p2(*r);

        // first, compare triangle area
        if (p1(2) < p2(2)) { return false; }
        if (p1(2) > p2(2)) { return true; }

        // same area, compare point position to get some stability
        if (p1(0) < p2(0)) { return false; }
        if (p1(0) > p2(0)) { return true; }

        return (p1(1) < p2(1));
    });

    auto heapBegin(heap.begin());
    auto heapEnd(heap.end());

    const auto reheap([&]() -> void
    {
        std::make_heap(heapBegin, heapEnd, compare);
    });

    const auto getPrev([&](Heap::value_type it) -> Heap::value_type
    {
        if (it == points.begin()) { return std::prev(points.end()); }
        return std::prev(it);
    });

    const auto getNext([&](Heap::value_type it) -> Heap::value_type
    {
        ++it;
        if (it == points.end()) { return points.begin(); }
        return it;
    });

    // simplify
    while (heapBegin != heapEnd) {
        // make sure we have a heap
        reheap();
        auto &point(*heapBegin);

        // check stop condition
        if ((*point)(2) > stopCondition) { break; }

        // remove pointer for list of points
        point = points.erase(point);
        if (point == points.end()) { point = points.begin(); }
        auto prev(getPrev(point));

        if (std::isfinite((*prev)(2))) {
            (*prev)(2) = area(*getPrev(prev), *prev, *point);
        }
        if (std::isfinite((*point)(2))) {
            (*point)(2) = area(*prev, *point, *getNext(point));
        }

        // cut head from heap and recompute
        ++heapBegin;
    }

    math::Polygon out;
    for (const auto &p : points) { out.emplace_back(p(0), p(1)); }
    return out;
}

} // namespace

Contour::list simplify(Contour::list contours)
{
    // first, we need to identify points where more than two contours join
    const auto lockedPoints(findLockedPoints(contours));

    for (auto &contour : contours) {
        if (!contour) { continue; }

        for (auto &ring : contour.rings) {
            ring = simplifyRing(ring, lockedPoints, 10.0);
        }
    }

    return contours;
}

} // namespace imgproc

#endif // imgproc_contour_hpp_included_
