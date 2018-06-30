#pragma once

#include "../Mesh.hh"

namespace polymesh
{
inline vertex_index Mesh::add_vertex()
{
    auto idx = vertex_index((int)mVertices.size());
    mVertices.push_back(vertex_info());

    // notify attributes
    auto vCnt = (int)mVertices.size();
    for (auto p = mVertexAttrs; p; p = p->mNextAttribute)
        p->resize(vCnt, false);

    return idx;
}

inline face_index Mesh::add_face(const vertex_handle *v_handles, int vcnt)
{
    mFaceInsertCache.resize(vcnt);
    for (auto i = 0; i < vcnt; ++i)
        mFaceInsertCache[i] = add_or_get_halfedge(v_handles[i].idx, v_handles[(i + 1) % vcnt].idx);
    return add_face(mFaceInsertCache.data(), vcnt);
}

inline face_index Mesh::add_face(const vertex_index *v_indices, int vcnt)
{
    mFaceInsertCache.resize(vcnt);
    for (auto i = 0; i < vcnt; ++i)
        mFaceInsertCache[i] = add_or_get_halfedge(v_indices[i], v_indices[(i + 1) % vcnt]);
    return add_face(mFaceInsertCache.data(), vcnt);
}

inline face_index Mesh::add_face(const halfedge_handle *half_loop, int vcnt)
{
    mFaceInsertCache.resize(vcnt);
    for (auto i = 0; i < vcnt; ++i)
        mFaceInsertCache[i] = half_loop[i].idx;
    return add_face(mFaceInsertCache.data(), vcnt);
}

inline face_index Mesh::add_face(const halfedge_index *half_loop, int vcnt)
{
    assert(vcnt >= 3 && "no support for less-than-triangular faces");

    auto fidx = face_index((int)mFaces.size());

    // ensure that half-edges are adjacent at each vertex
    for (auto i = 0; i < vcnt; ++i)
    {
        auto h0 = half_loop[i];
        auto h1 = half_loop[(i + 1) % vcnt];

        // half-edge must form a chain
        assert(to_vertex_of(h0) == from_vertex_of(h1) && "half-edges do not form a chain");
        // half-edge must be free, i.e. allow a new polygon
        assert(halfedge(h0).is_free() && "half-edge already contains a face");

        // make them adjacent
        make_adjacent(h0, h1);

        // link face
        halfedge(h0).face = fidx;
    }

    // fix boundary states
    for (auto i = 0; i < vcnt; ++i)
    {
        auto h = half_loop[i];
        auto v = halfedge(h).to_vertex;
        auto f = halfedge(opposite(h)).face;

        // fix vertex
        fix_boundary_state_of(v);

        // fix face
        if (f.is_valid())
            fix_boundary_state_of(f);
    }

    // set up face data
    face_info f;
    f.halfedge = half_loop[0];
    mFaces.push_back(f);

    // fix new face
    fix_boundary_state_of(fidx);

    // notify attributes
    auto fCnt = (int)mFaces.size();
    for (auto p = mFaceAttrs; p; p = p->mNextAttribute)
        p->resize(fCnt, false);

    return fidx;
}

inline edge_index Mesh::add_or_get_edge(vertex_index v_from, vertex_index v_to)
{
    assert(v_from != v_to);

    // already exists?
    auto he = find_halfedge(v_from, v_to);
    if (he.is_valid())
        return edge_of(he);

    auto &vd_from = vertex(v_from);
    auto &vd_to = vertex(v_to);

    // allocate new
    auto he_size = (int)mHalfedges.size();
    auto h_from_to_idx = halfedge_index(he_size + 0);
    auto h_to_from_idx = halfedge_index(he_size + 1);
    auto eidx = edge_index(he_size >> 1);
    halfedge_info h_from_to;
    halfedge_info h_to_from;

    // setup data (self-connected edge)
    h_from_to.to_vertex = v_to;
    h_to_from.to_vertex = v_from;
    h_from_to.next_halfedge = h_to_from_idx;
    h_to_from.next_halfedge = h_from_to_idx;

    // link from vertex
    if (vd_from.is_isolated())
        vd_from.outgoing_halfedge = h_from_to_idx;
    else
    {
        auto from_in_idx = find_free_incident(v_from);
        assert(from_in_idx.is_valid() && "vertex is already fully connected");
        auto &from_in = halfedge(from_in_idx);
        auto from_out_idx = from_in.next_halfedge;
        auto &from_out = halfedge(from_out_idx);

        from_in.next_halfedge = h_from_to_idx;
        h_from_to.prev_halfedge = from_in_idx;

        h_to_from.next_halfedge = from_out_idx;
        from_out.prev_halfedge = h_to_from_idx;
    }

    // link to vertex
    if (vd_to.is_isolated())
        vd_to.outgoing_halfedge = h_to_from_idx;
    else
    {
        auto to_in_idx = find_free_incident(v_to);
        assert(to_in_idx.is_valid() && "vertex is already fully connected");
        auto &to_in = halfedge(to_in_idx);
        auto to_out_idx = to_in.next_halfedge;
        auto &to_out = halfedge(to_out_idx);

        to_in.next_halfedge = h_to_from_idx;
        h_to_from.prev_halfedge = to_in_idx;

        h_from_to.next_halfedge = to_out_idx;
        to_out.prev_halfedge = h_from_to_idx;
    }

    // finalize
    mHalfedges.push_back(h_from_to);
    mHalfedges.push_back(h_to_from);

    // notify attributes
    auto hCnt = (int)mHalfedges.size();
    auto eCnt = hCnt >> 1;
    for (auto p = mHalfedgeAttrs; p; p = p->mNextAttribute)
        p->resize(hCnt, false);
    for (auto p = mEdgeAttrs; p; p = p->mNextAttribute)
        p->resize(eCnt, false);

    return eidx;
}

inline halfedge_index Mesh::add_or_get_halfedge(vertex_index v_from, vertex_index v_to)
{
    auto e = add_or_get_edge(v_from, v_to);
    auto h0 = halfedge_of(e, 0);
    auto h1 = halfedge_of(e, 1);
    return halfedge(h0).to_vertex == v_to ? h0 : h1;
}

inline void Mesh::make_adjacent(halfedge_index he_in, halfedge_index he_out)
{
    // see http://kaba.hilvi.org/homepage/blog/halfedge/halfedge.htm ::makeAdjacent
    auto &in = halfedge(he_in);
    auto &out = halfedge(he_out);

    auto he_b = in.next_halfedge;
    auto he_d = out.prev_halfedge;

    // already correct
    if (he_b == he_out)
        return;

    // find free half-edge after `out` but before `in`
    auto he_g = find_free_incident(opposite(he_out), he_in);
    assert(he_g.is_valid()); // unable to make adjacent

    auto &b = halfedge(he_b);
    auto &d = halfedge(he_d);
    auto &g = halfedge(he_g);

    auto he_h = g.next_halfedge;
    auto &h = halfedge(he_h);

    // properly rewire
    in.next_halfedge = he_out;
    out.prev_halfedge = he_in;

    g.next_halfedge = he_b;
    b.prev_halfedge = he_g;

    d.next_halfedge = he_h;
    h.prev_halfedge = he_d;
}

inline void Mesh::remove_face(face_index f_idx)
{
    auto &f = face(f_idx);
    assert(f.halfedge.is_valid());

    auto he_begin = f.halfedge;
    auto he = he_begin;
    do
    {
        auto &h = halfedge(he);
        assert(h.face == f_idx);

        // set half-edge face to invalid
        h.face = face_index::invalid();

        // fix outgoing vertex half-edge
        // (vertex correctly reports is_boundary)
        vertex(from_vertex_of(he)).outgoing_halfedge = he;

        // fix opposite face half-edge
        auto ohe = opposite(he);
        auto of = halfedge(ohe).face;
        if (of.is_valid())
            face(of).halfedge = ohe;

        // advance
        he = h.next_halfedge;
    } while (he != he_begin);

    // mark removed
    // (at the end!)
    f.set_removed();

    // bookkeeping
    mRemovedFaces++;
    mCompact = false;
}

inline void Mesh::remove_edge(edge_index e_idx)
{
    auto hi_in = halfedge_of(e_idx, 0);
    auto hi_out = halfedge_of(e_idx, 1);

    auto &h_in = halfedge(hi_in);
    auto &h_out = halfedge(hi_out);

    assert(h_in.is_valid());
    assert(h_out.is_valid());

    auto f0 = h_in.face;
    auto f1 = h_out.face;

    // remove adjacent faces
    if (f0.is_valid())
        remove_face(f0);
    if (f1.is_valid())
        remove_face(f1);

    // rewire vertices
    auto &v_in_to = vertex(h_in.to_vertex);
    auto &v_out_to = vertex(h_out.to_vertex);

    auto hi_out_prev = h_out.prev_halfedge;
    auto hi_out_next = h_out.next_halfedge;

    auto hi_in_prev = h_in.prev_halfedge;
    auto hi_in_next = h_in.next_halfedge;

    // modify vertex if outgoing half-edge is going to be removed
    if (v_in_to.outgoing_halfedge == hi_out)
    {
        if (hi_in_next == hi_out) // v_in_to becomes isolated
            v_in_to.outgoing_halfedge = halfedge_index::invalid();
        else
            v_in_to.outgoing_halfedge = hi_in_next;
    }

    if (v_out_to.outgoing_halfedge == hi_in)
    {
        if (hi_out_next == hi_in) // v_out_to becomes isolated
            v_out_to.outgoing_halfedge = halfedge_index::invalid();
        else
            v_out_to.outgoing_halfedge = hi_out_next;
    }

    // reqire half-edges
    halfedge(hi_out_prev).next_halfedge = hi_in_next;
    halfedge(hi_in_next).prev_halfedge = hi_out_prev;

    halfedge(hi_in_prev).next_halfedge = hi_out_next;
    halfedge(hi_out_next).prev_halfedge = hi_in_prev;

    // remove half-edges
    h_in.set_removed();
    h_out.set_removed();

    // bookkeeping
    mRemovedHalfedges++;
    mRemovedHalfedges++;
    mCompact = false;
}

inline void Mesh::remove_vertex(vertex_index v_idx)
{
    auto &v = vertex(v_idx);
    assert(v.is_valid());

    // remove all outgoing edges
    while (!v.is_isolated())
        remove_edge(edge_of(v.outgoing_halfedge));

    // mark removed
    v.set_removed();

    // bookkeeping
    mRemovedVertices++;
    mCompact = false;
}

inline void Mesh::fix_boundary_state_of(vertex_index v_idx)
{
    auto &v = vertex(v_idx);
    assert(!v.is_isolated());

    auto he_begin = v.outgoing_halfedge;
    auto he = he_begin;
    do
    {
        // if half-edge is boundary, set it
        if (halfedge(he).is_free())
        {
            v.outgoing_halfedge = he;
            return;
        }

        // advance
        he = halfedge(opposite(he)).next_halfedge;
    } while (he != he_begin);
}

inline void Mesh::fix_boundary_state_of(face_index f_idx)
{
    auto &f = face(f_idx);

    auto he_begin = f.halfedge;
    auto he = he_begin;
    do
    {
        // if half-edge is boundary, set it
        if (halfedge(opposite(he)).is_free())
        {
            f.halfedge = he;
            return;
        }

        // advance
        he = halfedge(he).next_halfedge;
    } while (he != he_begin);
}

inline halfedge_index Mesh::find_free_incident(halfedge_index in_begin, halfedge_index in_end) const
{
    assert(halfedge(in_begin).to_vertex == halfedge(in_end).to_vertex);

    auto he = in_begin;
    do
    {
        auto const &h = halfedge(he);
        assert(h.to_vertex == halfedge(in_end).to_vertex);

        // free? found one!
        if (h.is_free())
            return he;

        // next half-edge of vertex
        he = opposite(h.next_halfedge);
    } while (he != in_end);

    return halfedge_index::invalid();
}

inline halfedge_index Mesh::find_free_incident(vertex_index v) const
{
    auto in_begin = opposite(vertex(v).outgoing_halfedge);
    return find_free_incident(in_begin, in_begin);
}

inline halfedge_index Mesh::find_halfedge(vertex_index from, vertex_index to) const
{
    auto he_begin = vertex(from).outgoing_halfedge;
    if (!he_begin.is_valid())
        return halfedge_index::invalid(); // isolated vertex

    auto he = he_begin;
    do
    {
        auto const &h = halfedge(he);

        // found?
        if (h.to_vertex == to)
            return he;

        // advance
        he = halfedge(opposite(he)).next_halfedge;

    } while (he != he_begin);

    return halfedge_index::invalid(); // not found
}

inline bool Mesh::is_boundary(vertex_index idx) const
{
    auto const &v = vertex(idx);
    return v.outgoing_halfedge.is_valid() && is_boundary(v.outgoing_halfedge);
}

inline bool Mesh::is_boundary(halfedge_index idx) const { return halfedge(idx).is_free(); }

inline halfedge_index Mesh::opposite(halfedge_index he) const { return halfedge_index(he.value ^ 1); }

inline vertex_index Mesh::next_valid_idx_from(vertex_index idx) const
{
    for (auto i = idx.value; i < (int)mVertices.size(); ++i)
        if (mVertices[i].is_valid())
            return vertex_index(i);
    return vertex_index(size_vertices()); // end index
}

inline vertex_index Mesh::prev_valid_idx_from(vertex_index idx) const
{
    for (auto i = idx.value; i >= 0; --i)
        if (mVertices[i].is_valid())
            return vertex_index(i);
    return {}; // invalid
}

inline edge_index Mesh::next_valid_idx_from(edge_index idx) const
{
    for (auto i = idx.value << 1; i < (int)mHalfedges.size(); i += 2)
        if (mHalfedges[i].is_valid())
            return edge_index(i >> 1);
    return edge_index(size_edges()); // end index
}

inline edge_index Mesh::prev_valid_idx_from(edge_index idx) const
{
    for (auto i = idx.value << 1; i >= 0; i -= 2)
        if (mHalfedges[i].is_valid())
            return edge_index(i >> 1);
    return {}; // invalid
}

inline face_index Mesh::next_valid_idx_from(face_index idx) const
{
    for (auto i = idx.value; i < (int)mFaces.size(); ++i)
        if (mFaces[i].is_valid())
            return face_index(i);
    return face_index(size_faces()); // end index
}

inline face_index Mesh::prev_valid_idx_from(face_index idx) const
{
    for (auto i = idx.value; i >= 0; --i)
        if (mFaces[i].is_valid())
            return face_index(i);
    return {}; // invalid
}

inline halfedge_index Mesh::next_valid_idx_from(halfedge_index idx) const
{
    for (auto i = idx.value; i < (int)mHalfedges.size(); ++i)
        if (mHalfedges[i].is_valid())
            return halfedge_index(i);
    return halfedge_index(size_halfedges()); // end index
}

inline halfedge_index Mesh::prev_valid_idx_from(halfedge_index idx) const
{
    for (auto i = idx.value; i >= 0; --i)
        if (mHalfedges[i].is_valid())
            return halfedge_index(i);
    return {}; // invalid
}

inline void Mesh::compactify()
{
    if (is_compact())
        return;

    // calculate remappings
    int v_cnt = size_vertices();
    int f_cnt = size_faces();
    int e_cnt = size_edges();
    int h_cnt = size_halfedges();
    std::vector<int> v_new_to_old;
    std::vector<int> f_new_to_old;
    std::vector<int> e_new_to_old;
    std::vector<int> h_new_to_old;
    v_new_to_old.reserve(v_cnt);
    f_new_to_old.reserve(f_cnt);
    e_new_to_old.reserve(e_cnt);
    h_new_to_old.reserve(h_cnt);
    std::vector<int> h_old_to_new(h_cnt, -1);
    std::vector<int> v_old_to_new(v_cnt, -1);
    std::vector<int> f_old_to_new(f_cnt, -1);

    for (auto i = 0; i < v_cnt; ++i)
        if (mVertices[i].is_valid())
        {
            v_old_to_new[i] = (int)v_new_to_old.size();
            v_new_to_old.push_back(i);
        }

    for (auto i = 0; i < f_cnt; ++i)
        if (mFaces[i].is_valid())
        {
            f_old_to_new[i] = (int)f_new_to_old.size();
            f_new_to_old.push_back(i);
        }

    for (auto i = 0; i < e_cnt; ++i)
        if (mHalfedges[i << 1].is_valid())
            e_new_to_old.push_back(i);

    for (auto i = 0; i < h_cnt; ++i)
        if (mHalfedges[i].is_valid())
        {
            h_old_to_new[i] = (int)h_new_to_old.size();
            h_new_to_old.push_back(i);
        }

    // apply remappings (map[new_prim_id] = old_prim_id)

    for (auto i = 0u; i < v_new_to_old.size(); ++i)
        mVertices[i] = mVertices[v_new_to_old[i]];
    for (auto i = 0u; i < f_new_to_old.size(); ++i)
        mFaces[i] = mFaces[f_new_to_old[i]];
    for (auto i = 0u; i < h_new_to_old.size(); ++i)
        mHalfedges[i] = mHalfedges[h_new_to_old[i]];

    mVertices.resize(v_new_to_old.size());
    mFaces.resize(f_new_to_old.size());
    mHalfedges.resize(h_new_to_old.size());

    for (auto &v : mVertices)
    {
        if (v.outgoing_halfedge.value >= 0)
            v.outgoing_halfedge.value = h_old_to_new[v.outgoing_halfedge.value];
    }

    for (auto &f : mFaces)
    {
        if (f.halfedge.value >= 0)
            f.halfedge.value = h_old_to_new[f.halfedge.value];
    }

    for (auto &h : mHalfedges)
    {
        if (h.next_halfedge.value >= 0)
            h.next_halfedge.value = h_old_to_new[h.next_halfedge.value];
        if (h.prev_halfedge.value >= 0)
            h.prev_halfedge.value = h_old_to_new[h.prev_halfedge.value];
        if (h.face.value >= 0)
            h.face.value = f_old_to_new[h.face.value];
        if (h.to_vertex.value >= 0)
            h.to_vertex.value = v_old_to_new[h.to_vertex.value];
    }

    for (auto a = mVertexAttrs; a; a = a->mNextAttribute)
        a->apply_remapping(v_new_to_old);
    for (auto a = mFaceAttrs; a; a = a->mNextAttribute)
        a->apply_remapping(f_new_to_old);
    for (auto a = mEdgeAttrs; a; a = a->mNextAttribute)
        a->apply_remapping(e_new_to_old);
    for (auto a = mHalfedgeAttrs; a; a = a->mNextAttribute)
        a->apply_remapping(h_new_to_old);

    // shrink to fit
    mVertices.shrink_to_fit();
    mFaces.shrink_to_fit();
    mHalfedges.shrink_to_fit();

    for (auto a = mVertexAttrs; a; a = a->mNextAttribute)
        a->resize(size_vertices(), true);
    for (auto a = mFaceAttrs; a; a = a->mNextAttribute)
        a->resize(size_faces(), true);
    for (auto a = mEdgeAttrs; a; a = a->mNextAttribute)
        a->resize(size_edges(), true);
    for (auto a = mHalfedgeAttrs; a; a = a->mNextAttribute)
        a->resize(size_halfedges(), true);

    mRemovedFaces = 0;
    mRemovedHalfedges = 0;
    mRemovedVertices = 0;
    mCompact = true;
}

inline void Mesh::clear()
{
    for (auto &v : mVertices)
        v.set_removed();
    for (auto &h : mHalfedges)
        h.set_removed();
    for (auto &f : mFaces)
        f.set_removed();

    mCompact = false;
    compactify();
}

inline void Mesh::reserve_faces(int capacity)
{
    for (auto a = mFaceAttrs; a; a = a->mNextAttribute)
        a->resize(capacity, false);

    mFaces.reserve(capacity);
}

inline void Mesh::reserve_vertices(int capacity)
{
    for (auto a = mVertexAttrs; a; a = a->mNextAttribute)
        a->resize(capacity, false);

    mVertices.reserve(capacity);
}

inline void Mesh::reserve_edges(int capacity)
{
    for (auto a = mEdgeAttrs; a; a = a->mNextAttribute)
        a->resize(capacity, false);
    for (auto a = mHalfedgeAttrs; a; a = a->mNextAttribute)
        a->resize(capacity << 1, false);

    mHalfedges.reserve(capacity * 2);
}

inline void Mesh::reserve_halfedges(int capacity)
{
    for (auto a = mHalfedgeAttrs; a; a = a->mNextAttribute)
        a->resize(capacity, false);
    for (auto a = mEdgeAttrs; a; a = a->mNextAttribute)
        a->resize(capacity >> 1, false);

    mHalfedges.reserve(capacity);
}
}
