#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <vector>

// Helper for mesh-based attribute bookkeeping

namespace polymesh
{
class Mesh;

template <class MeshT>
struct low_level_api_base;

template <class tag>
struct primitive_attribute_base
{
private:
    primitive_attribute_base* mNextAttribute = nullptr;
    primitive_attribute_base* mPrevAttribute = nullptr;

protected:
    Mesh const* mMesh;
    primitive_attribute_base(Mesh const* mesh) : mMesh(mesh) {} // no registration, it's too early!
    virtual void resize_from(int old_size) = 0;
    virtual void apply_remapping(std::vector<int> const& map) = 0;
    virtual void apply_transpositions(std::vector<std::pair<int, int>> const& ts) = 0;

    // links and unlinks the attribute with the mesh
    // CAUTION: does not change data in any way.
    void register_attr();
    void deregister_attr();

    friend class Mesh;

    template <class MeshT>
    friend struct low_level_api_base;

public:
    virtual ~primitive_attribute_base()
    {
        // CAUTION: this must always be called in case attributes get non-default ctors
        deregister_attr();
    }

    Mesh const& mesh() const { return *mMesh; }
};
} // namespace polymesh
