﻿#pragma once

#include <vector>

#include "gpc_bbox.hpp"
#include "gpc_vertex.hpp"

namespace gpc {

// Vertex list structure
class gpc_vertex_list
{
public:
    bool is_contributing = true;
    bool is_hole = false;
    std::vector<gpc_vertex> vertexs; // Vertex array pointer

public:
    gpc_vertex_list() = default;
    gpc_vertex_list(const std::vector<gpc_vertex> &in_vertex, bool hole = false)
        : is_hole(hole), vertexs(in_vertex)
    {
    }
    ~gpc_vertex_list() = default;

    inline int num_vertices() const { return vertexs.size(); }

    bool operator==(const gpc_vertex_list &rhs) const;
    inline bool operator!=(const gpc_vertex_list &rhs) const
    {
        return !(*this == rhs);
    }

    friend std::istream &operator>>(std::istream &is,
                                    gpc_vertex_list &vertex_list);
    friend std::ostream &operator<<(std::ostream &os,
                                    const gpc_vertex_list &vertex_list);

    gpc_bbox create_bbox() const;

    std::string to_string() const;
};

} // namespace gpc