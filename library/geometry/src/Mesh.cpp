#include "Mesh.h"
#include "InternalMesh.h"

namespace geometry {

Mesh::Mesh() :
    m_mesh(std::make_unique<InternalMesh>())
{
}

Mesh::~Mesh() = default;

bool Mesh::is_triangulated() const
{
   assert(m_mesh != nullptr);
   return m_mesh->is_triangulated();
}

void Mesh::recalculate_normals() const
{
   assert(m_mesh != nullptr);
   return m_mesh->recalculate_normals();
}

void Mesh::triangulate() const
{
   assert(m_mesh != nullptr);
   m_mesh->triangulate_faces();
}

Index Mesh::add_vertex(float x, float y, float z)
{
   assert(m_mesh != nullptr);
   return m_mesh->add_vertex({x, y, z}).id();
}

Index Mesh::add_face_range(std::span<Index> vertices)
{
   std::vector<InternalMesh::VertexIndex> vecVertices{};
   vecVertices.resize(vertices.size());
   std::ranges::transform(vertices, vecVertices.begin(),
                          [](const Index i) { return InternalMesh::VertexIndex{i}; });
   return m_mesh->add_face(vecVertices);
}

Index Mesh::add_uv(float u, float v)
{
   assert(m_mesh != nullptr);
   return m_mesh->add_uv({u, v});
}

Index Mesh::add_normal(float x, float y, float z)
{
   assert(m_mesh != nullptr);
   return m_mesh->add_normal({x, y, z});
}

size_t Mesh::vertex_count()
{
   assert(m_mesh != nullptr);
   return m_mesh->vertex_count();
}

void Mesh::set_face_uvs_range(const Index face, const std::span<Index> vertices)
{
   assert(m_mesh != nullptr);
   return m_mesh->set_face_uvs(face, vertices);
}

void Mesh::set_face_normals_range(Index face, std::span<Index> vertices)
{
   assert(m_mesh != nullptr);
   return m_mesh->set_face_normals(face, vertices);
}

void Mesh::reverse_orientation()
{
   assert(m_mesh != nullptr);
   m_mesh->reverse_orientation();
}

graphics_api::Mesh<Vertex> Mesh::upload_to_device(graphics_api::Device &device) const
{
   assert(m_mesh != nullptr);
   return m_mesh->upload_to_device(device);
}

Mesh Mesh::from_file(const std::string_view path)
{
   auto internalMesh = InternalMesh::from_obj_file(path);
   return Mesh(std::make_unique<InternalMesh>(std::move(internalMesh)));
}

Mesh::Mesh(std::unique_ptr<InternalMesh> mesh) :
   m_mesh(std::move(mesh))
{
}

}// namespace geometry