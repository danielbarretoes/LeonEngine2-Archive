#include <doctest/doctest.h>

#include "Assets/UStaticMesh.hpp"
#include "Physics/FCollisionQuery.hpp"

#include <glm/glm.hpp>

using namespace Leon;

namespace {

    TRef<UStaticMesh> MakeRightTriangleFloor() {
        auto mesh = UStaticMesh::Create("TriFloor");
        FStaticMeshVertex a{};
        a.Position = {0.0f, 0.0f, 0.0f};
        a.Normal = {0.0f, 1.0f, 0.0f};
        FStaticMeshVertex b = a;
        b.Position = {4.0f, 0.0f, 0.0f};
        FStaticMeshVertex c = a;
        c.Position = {0.0f, 0.0f, 4.0f};
        mesh->GetVertices() = {a, b, c};
        mesh->GetIndices() = {0, 1, 2};
        mesh->CalculateBounds();
        return mesh;
    }

    FColliderDesc MakeTriCollider(const UStaticMesh& InMesh) {
        FColliderDesc d;
        d.Shape = EPhysicsShapeType::TriangleMesh;
        d.TriangleMesh = &InMesh;
        d.TriangleWorld = glm::mat4(1.0f);
        d.Center = (InMesh.GetBoundsMin() + InMesh.GetBoundsMax()) * 0.5f;
        d.BoxHalfExtent = (InMesh.GetBoundsMax() - InMesh.GetBoundsMin()) * 0.5f;
        d.ObjectType = ECollisionChannel::WorldStatic;
        return d;
    }

} // namespace

TEST_SUITE("TriangleMeshCollision") {
    TEST_CASE("ray hits triangle face and misses the empty AABB corner") {
        auto mesh = MakeRightTriangleFloor();
        FColliderDesc col = MakeTriCollider(*mesh);
        FHitResult hit;
        CHECK(RaycastCollider(col, glm::vec3(0.4f, 2.0f, 0.4f), glm::vec3(0.0f, -1.0f, 0.0f), 8.0f, hit));
        CHECK(hit.bBlockingHit);
        CHECK(hit.Location.y == doctest::Approx(0.0f).epsilon(0.02f));

        FHitResult miss;
        CHECK_FALSE(RaycastCollider(col, glm::vec3(3.6f, 2.0f, 3.6f), glm::vec3(0.0f, -1.0f, 0.0f), 8.0f, miss));
    }

    TEST_CASE("sphere sweep rests on triangle vertices") {
        auto mesh = MakeRightTriangleFloor();
        FColliderDesc col = MakeTriCollider(*mesh);
        FHitResult hit;
        CHECK(SweepSphereCollider(col, glm::vec3(0.5f, 2.0f, 0.5f), glm::vec3(0.5f, -1.0f, 0.5f), 0.4f, hit));
        CHECK(hit.bBlockingHit);
        CHECK(hit.Distance < 2.0f);
    }
}
