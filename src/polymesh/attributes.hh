#pragma once

#include <cstddef>
#include <vector>

#include "attribute_base.hh"
#include "cursors.hh"
#include "tmp.hh"

/** Attributes
 *
 * Golden rule:
 *  - the Mesh must always outlive the attribute!
 *
 * Create attributes:
 *   auto myAttr = mesh.vertices().make_attribute(0.0f);
 *
 * Access attributes:
 *   vertex_handle v; // or _index
 *   v[myAttr] = 7;
 *   myAttr[v] = 7;
 */

namespace polymesh
{
template <class tag, class AttrT>
struct primitive_attribute : primitive_attribute_base<tag>
{
    template <class A>
    using attribute = typename primitive<tag>::template attribute<A>;
    using index_t = typename primitive<tag>::index;
    using handle_t = typename primitive<tag>::handle;

    // data access
public:
    AttrT& operator[](handle_t v) { return mData[v.idx.value]; }
    AttrT const& operator[](handle_t v) const { return mData[v.idx.value]; }
    AttrT& operator[](index_t v) { return mData[v.value]; }
    AttrT const& operator[](index_t v) const { return mData[v.value]; }

    AttrT* data() { return mData.data; }
    AttrT const* data() const { return mData.data; }
    int size() const;

    // methods
public:
    void clear(AttrT const& value);
    void clear();

    /// returns a new attribute where the given function was applied to each entry
    template <class FuncT>
    auto map(FuncT f) const -> attribute<tmp::result_type_of<FuncT, AttrT>>;
    /// applies to given function to each attribute entry
    template <class FuncT>
    void apply(FuncT f);

    // data
protected:
    attribute_data<AttrT> mData;
    AttrT mDefaultValue;

    void on_resize(int newSize) override { mData.resize(newSize, mDefaultValue); }
    void apply_remapping(std::vector<int> const& map) override;

    // ctor
protected:
    primitive_attribute(Mesh const* mesh, AttrT const& def_value);

    // move & copy
public:
    primitive_attribute(primitive_attribute const&);
    primitive_attribute(primitive_attribute&&);
    primitive_attribute& operator=(primitive_attribute const&);
    primitive_attribute& operator=(primitive_attribute&&);
};

template <class AttrT>
struct vertex_attribute : primitive_attribute<vertex_tag, AttrT>
{
    using primitive_attribute<vertex_tag, AttrT>::primitive_attribute;

    friend struct vertex_collection;
    friend struct const_vertex_collection;
};
template <class AttrT>
struct face_attribute : primitive_attribute<face_tag, AttrT>
{
    using primitive_attribute<face_tag, AttrT>::primitive_attribute;

    friend struct face_collection;
    friend struct const_face_collection;
};
template <class AttrT>
struct edge_attribute : primitive_attribute<edge_tag, AttrT>
{
    using primitive_attribute<edge_tag, AttrT>::primitive_attribute;

    friend struct edge_collection;
    friend struct const_edge_collection;
};
template <class AttrT>
struct halfedge_attribute : primitive_attribute<halfedge_tag, AttrT>
{
    using primitive_attribute<halfedge_tag, AttrT>::primitive_attribute;

    friend struct halfedge_collection;
    friend struct const_halfedge_collection;
};

/// ======== IMPLEMENTATION ========

template <class tag, class AttrT>
void primitive_attribute<tag, AttrT>::apply_remapping(const std::vector<int>& map)
{
    for (auto i = 0u; i < map.size(); ++i)
        this->mData[i] = this->mData[map[i]];
}

template <class tag, class AttrT>
primitive_attribute<tag, AttrT>::primitive_attribute(primitive_attribute const& rhs) : primitive_attribute_base<tag>(rhs.mMesh) // copy
{
    this->mDefaultValue = rhs.mDefaultValue;
    this->mData = rhs.mData;
    this->mDataSize = rhs.mDataSize;

    this->register_attr();
}

template <class tag, class AttrT>
primitive_attribute<tag, AttrT>::primitive_attribute(primitive_attribute&& rhs) : primitive_attribute_base<tag>(rhs.mMesh) // move
{
    this->mDefaultValue = std::move(rhs.mDefaultValue);
    this->mData = std::move(rhs.mData);
    this->mDataSize = rhs.mDataSize;

    rhs.deregister_attr();
    this->register_attr();
}

template <class tag, class AttrT>
primitive_attribute<tag, AttrT>& primitive_attribute<tag, AttrT>::operator=(primitive_attribute const& rhs) // copy
{
    this->deregister_attr();

    this->mMesh = rhs.mMesh;
    this->mDefaultValue = rhs.mDefaultValue;
    this->mData = rhs.mData;
    this->mDataSize = rhs.mDataSize;

    this->register_attr();
}

template <class tag, class AttrT>
primitive_attribute<tag, AttrT>& primitive_attribute<tag, AttrT>::operator=(primitive_attribute&& rhs) // move
{
    this->deregister_attr();

    this->mMesh = rhs.mMesh;
    this->mDefaultValue = std::move(rhs.mDefaultValue);
    this->mData = std::move(rhs.mData);
    this->mDataSize = rhs.mDataSize;

    rhs.deregister_attr();
    this->register_attr();
}

/// ======== CURSOR IMPLEMENTATION ========

template <class tag>
template <class AttrT>
AttrT& primitive_index<tag>::operator[](primitive_index::attribute<AttrT>& attr) const
{
    return attr[*this];
}
template <class tag>
template <class AttrT>
AttrT const& primitive_index<tag>::operator[](primitive_index::attribute<AttrT> const& attr) const
{
    return attr[*this];
}
template <class tag>
template <class AttrT>
AttrT& primitive_index<tag>::operator[](primitive_index::attribute<AttrT>* attr) const
{
    return (*attr)[*this];
}
template <class tag>
template <class AttrT>
AttrT const& primitive_index<tag>::operator[](primitive_index::attribute<AttrT> const* attr) const
{
    return (*attr)[*this];
}

template <class tag>
template <class AttrT>
AttrT& primitive_handle<tag>::operator[](primitive_handle::attribute<AttrT>& attr) const
{
    return attr[idx];
}
template <class tag>
template <class AttrT>
AttrT const& primitive_handle<tag>::operator[](primitive_handle::attribute<AttrT> const& attr) const
{
    return attr[idx];
}
template <class tag>
template <class AttrT>
AttrT& primitive_handle<tag>::operator[](primitive_handle::attribute<AttrT>* attr) const
{
    return (*attr)[idx];
}
template <class tag>
template <class AttrT>
AttrT const& primitive_handle<tag>::operator[](primitive_handle::attribute<AttrT> const* attr) const
{
    return (*attr)[idx];
}

}
