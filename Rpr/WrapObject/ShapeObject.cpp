/**********************************************************************
Copyright (c) 2016 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
********************************************************************/

#include <vector>

#include "WrapObject/ShapeObject.h"
#include "WrapObject/Exception.h"
#include "App/Scene/shape.h"



using namespace Baikal;

namespace
{
    template<typename T, int size = 3> std::vector<T> merge(const T* in_data, size_t in_data_num, rpr_int in_data_stride,
        rpr_int const * in_data_indices, rpr_int in_didx_stride,
        rpr_int const * in_num_face_vertices, size_t in_num_faces)
    {
        std::vector<T> result;
        int indent = 0;
        for (int i = 0; i < in_num_faces; ++i)
        {
            int face = in_num_face_vertices[i];
            for (int f = indent; f < face + indent; ++f)
            {
                int index = in_data_stride / sizeof(T) * in_data_indices[f*in_didx_stride / sizeof(rpr_int)];
                for (int s = 0; s < size; ++s)
                {
                    result.push_back(in_data[index + s]);
                }
            }

            indent += face;
        }
        return result;
    }
}

ShapeObject::ShapeObject(Baikal::Shape* shape, bool is_instance)
    : m_shape(shape)
    , m_is_instance(is_instance)
{
}

ShapeObject::~ShapeObject()
{
    delete m_shape;
    m_shape = nullptr;
    m_is_instance = false;
}

ShapeObject* ShapeObject::CreateInstance()
{
    if (!m_shape)
    {
        return nullptr;
    }
    Instance* instance = nullptr;
    if (m_is_instance)
    {
        //get base shape
        Instance* inst = WrapObject::Cast<Instance>(m_shape);
        if (!inst)
        {
            //something goes wrong
            return nullptr;
        }
        const Shape* base = inst->GetBaseShape();
        instance = new Instance(base);
    }
    else
    {
        instance = new Instance(m_shape);
    }

    ShapeObject* result = new ShapeObject(instance, true);

    return result;
}

ShapeObject* ShapeObject::CreateMesh(rpr_float const * in_vertices, size_t in_num_vertices, rpr_int in_vertex_stride,
                        rpr_float const * in_normals, size_t in_num_normals, rpr_int in_normal_stride,
                        rpr_float const * in_texcoords, size_t in_num_texcoords, rpr_int in_texcoord_stride,
                        rpr_int const * in_vertex_indices, rpr_int in_vidx_stride,
                        rpr_int const * in_normal_indices, rpr_int in_nidx_stride,
                        rpr_int const * in_texcoord_indices, rpr_int in_tidx_stride,
                        rpr_int const * in_num_face_vertices, size_t in_num_faces)
{
    //merge data
    std::vector<float> verts = merge(in_vertices, in_num_vertices, in_vertex_stride,
        in_vertex_indices, in_vidx_stride,
        in_num_face_vertices, in_num_faces);
    std::vector<float> normals = merge(in_normals, in_num_normals, in_normal_stride,
        in_normal_indices, in_nidx_stride,
        in_num_face_vertices, in_num_faces);
    std::vector<float> uvs = merge<float, 2>(in_texcoords, in_num_texcoords, in_texcoord_stride,
        in_texcoord_indices, in_tidx_stride,
        in_num_face_vertices, in_num_faces);

    //generate indices
    std::vector<std::uint32_t> inds;
    std::uint32_t indent = 0;
    for (std::uint32_t i = 0; i < in_num_faces; ++i)
    {
        inds.push_back(indent);
        inds.push_back(indent + 1);
        inds.push_back(indent + 2);

        int face = in_num_face_vertices[i];

        //triangulation
        if (face == 4)
        {
            inds.push_back(indent + 0);
            inds.push_back(indent + 2);
            inds.push_back(indent + 3);

        }
        //only triangles and quads supported
        else if (face != 3)
        {
            throw Exception(RPR_ERROR_INVALID_PARAMETER, "ShapeObject: invalid face value.");
        }
        indent += face;
    }

    //create mesh
    Baikal::Mesh* mesh = new Baikal::Mesh();
    mesh->SetVertices(verts.data(), verts.size() / 3);
    mesh->SetNormals(normals.data(), normals.size() / 3);
    mesh->SetUVs(uvs.data(), uvs.size() / 2);
    mesh->SetIndices(inds.data(), inds.size());

    return new ShapeObject(mesh, false);
}
void ShapeObject::SetMaterial(MaterialObject* mat)
{
    if (mat)
    {
        if (!mat->IsMaterial())
        {
            throw Exception(RPR_ERROR_INVALID_PARAMETER, "ShapeObject: material is a texture.");
        }

        //handle fresnel materials
        if (mat->GetType() == MaterialObject::Type::kFresnel ||
            mat->GetType() == MaterialObject::Type::kFresnelShlick)
        {
            throw Exception(RPR_ERROR_INVALID_PARAMETER, "ShapeObject: fresnel materials available only as input for kBlend material.");
        }
        m_shape->SetMaterial(mat->GetMaterial());
    }
    else
    {
        m_shape->SetMaterial(nullptr);
    }
}