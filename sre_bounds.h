/*

Copyright (c) 2014 Harm Hanemaaijer <fgenfb@yahoo.com>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

*/


// This is a header file bounding volume handling and intersections tests that is
// internal to the library. Some intersection tests are declared static in
// intersection.cpp and not included in this header file; these can be made
// non-static and added to this header file when required by a different module.

// This header file contains useful inline functions for general use such as
// square (x * x), and the minimum and maximum of two or three values.

static inline float sqrf(float x) {
    return x * x;
}

static inline float minf(float x, float y) {
    if (x < y)
        return x;
    return y;
}

static inline float min3f(float x, float y, float z) {
    float m = x;
    if (y < m)
        m = y;
    if (z < m)
        m = z;
    return m;
}

static inline float maxf(float x, float y) {
    if (y > x)
        return y;
    return x;
}

static inline float max3f(float x, float y, float z) {
    float m = x;
    if (y > m)
        m = y;
    if (z > m)
        m = z;
    return m;
}

static inline float max3f(const Vector3D& V) {
    float m = V.x;
    if (V.y > m)
        m = V.y;
    if (V.z > m)
        m = V.z;
    return m;
}

static inline Vector3D maxf(const Vector3D& V1, const Vector3D& V2) {
    return Vector3D(maxf(V1.x, V2.x), maxf(V1.y, V2.y), maxf(V1.z, V2.z));
}

static inline float clampf(float x, float min_value, float max_value) {
    if (x < min_value)
        return min_value;
    if (x > max_value)
        return max_value;
    return x;
}   

// Inline AABB utility functions.

// Update AABB1 with the union of AABB1 and AABB2.

static inline void UpdateAABB(sreBoundingVolumeAABB& AABB1, const sreBoundingVolumeAABB& AABB2) {
    AABB1.dim_min.x = minf(AABB1.dim_min.x, AABB2.dim_min.x);
    AABB1.dim_max.x = maxf(AABB1.dim_max.x, AABB2.dim_max.x);
    AABB1.dim_min.y = minf(AABB1.dim_min.y, AABB2.dim_min.y);
    AABB1.dim_max.y = maxf(AABB1.dim_max.y, AABB2.dim_max.y);
    AABB1.dim_min.z = minf(AABB1.dim_min.z, AABB2.dim_min.z);
    AABB1.dim_max.z = maxf(AABB1.dim_max.z, AABB2.dim_max.z);
}

static inline void UpdateAABBWithIntersection(sreBoundingVolumeAABB& AABB1,
const sreBoundingVolumeAABB& AABB2) {
    AABB1.dim_min.x = maxf(AABB1.dim_min.x, AABB2.dim_min.x);
    AABB1.dim_max.x = minf(AABB1.dim_max.x, AABB2.dim_max.x);
    AABB1.dim_min.y = maxf(AABB1.dim_min.y, AABB2.dim_min.y);
    AABB1.dim_max.y = minf(AABB1.dim_max.y, AABB2.dim_max.y);
    AABB1.dim_min.z = maxf(AABB1.dim_min.z, AABB2.dim_min.z);
    AABB1.dim_max.z = minf(AABB1.dim_max.z, AABB2.dim_max.z);
}

// Extend AABB so that point P is part of it.

static inline void UpdateAABB(sreBoundingVolumeAABB& AABB, const Point3D& P) {
    AABB.dim_min.x = minf(AABB.dim_min.x, P.x);
    AABB.dim_max.x = maxf(AABB.dim_max.x, P.x);
    AABB.dim_min.y = minf(AABB.dim_min.y, P.y);
    AABB.dim_max.y = maxf(AABB.dim_max.y, P.y);
    AABB.dim_min.z = minf(AABB.dim_min.z, P.z);
    AABB.dim_max.z = maxf(AABB.dim_max.z, P.z);
}

// Utility functions and data structures for an array of bounding box vertices.

extern const int BB_plane_vertex[6][4];
extern const int flat_BB_plane_nu_vertices[6];
extern const int BB_edge_vertex[12][2];
extern const int BB_edge_plane[12][2];

static inline void MoveBoundingBoxVerticesInward(Point3D *P, int n_vertices, Vector4D *K, int plane, float dist) {
    int n;
    if (n_vertices == 4)
        n = flat_BB_plane_nu_vertices[plane];
    else
        n = 4;
    for (int i = 0; i < n; i++)
        P[BB_plane_vertex[plane][i]] += dist * K[plane].GetVector3D(); 
}


// Functions to calculate a bounding volume of another bounding volume of a different type.
// These are defined in bounding_volume.cpp.

static inline void CalculateAABB(const sreBoundingVolumeSphere& sphere, sreBoundingVolumeAABB& AABB) {
    AABB.dim_min = sphere.center - Vector3D(sphere.radius, sphere.radius, sphere.radius);
    AABB.dim_max = sphere.center + Vector3D(sphere.radius, sphere.radius, sphere.radius);
}

void CalculateAABB(const sreBoundingVolumeSphericalSector& spherical_sector,
    sreBoundingVolumeAABB& AABB);

void CalculateAABB(const sreBoundingVolumeCylinder& cylinder, sreBoundingVolumeAABB& AABB);

void CalculateBoundingSphere(const sreBoundingVolumeSphericalSector& spherical_sector,
    sreBoundingVolumeSphere& sphere);

void CalculateBoundingSphere(const sreBoundingVolumeCylinder& cylinder, sreBoundingVolumeSphere& sphere);

void CalculateBoundingCylinder(const sreBoundingVolumeSphericalSector& spherical_sector,
    sreBoundingVolumeCylinder& cylinder);

// Intersection test functions between bounding volumes, object bounds, light volume bounds, and
// octree bounds. Any test involving object, light volume or octree bounds is decomposed into
// bounding volume tests.

static inline bool Intersects(const sreBoundingVolumeAABB& AABB1, const sreBoundingVolumeAABB& AABB2) {
    if (AABB1.dim_min.x >= AABB2.dim_max.x || AABB1.dim_max.x <= AABB2.dim_min.x ||
    AABB1.dim_min.y >= AABB2.dim_max.y|| AABB1.dim_max.y <= AABB2.dim_min.y ||
    AABB1.dim_min.z >= AABB2.dim_max.z || AABB1.dim_max.z <= AABB2.dim_min.z)
        return false;
    return true;
}

static inline bool Intersects(const Point3D& P, const sreBoundingVolumeAABB& AABB) {
    return P.x >= AABB.dim_min.x && P.y >= AABB.dim_min.y && P.z >= AABB.dim_min.z &&
        P.x <=AABB.dim_max.x && P.y <= AABB.dim_max.y && P.z <= AABB.dim_max.z;
}

static inline bool Intersects(const Point3D& point, const sreBoundingVolumeConvexHull &ch) {
    for (int i = 0; i < ch.nu_planes; i++) {
        if (Dot(ch.plane[i], point) < 0)
            return false;
    }
    return true;
}

static inline bool Intersects(const sreBoundingVolumeSphere& sphere1,
const sreBoundingVolumeSphere& sphere2) {
    float dist_squared = SquaredMag(sphere1.center - sphere2.center);
    if (dist_squared >= sqrf(sphere1.radius + sphere2.radius))
        // The two spheres do not intersect.
        return false;
    return true;
}

static inline bool Intersects(const Point3D& P, const sreBoundingVolumeSphere& sphere) {
    if (SquaredMag(P - sphere.center) >= sqrf(sphere.radius))
        return false;
    return true;
}

bool Intersects(const Point3D& P, const sreBoundingVolumeBox& box);

bool Intersects(const sreBoundingVolumeBox& box, const sreBoundingVolumeSphere& sphere);

bool Intersects(const sreBoundingVolumeBox& box, const sreBoundingVolumeCylinder& cyl);

BoundsCheckResult QueryIntersection(const sreBoundingVolumeSphere& sphere,
    const sreBoundingVolumeCylinder& cyl);

// Intersection of a sphere and a convex hull. This test may miss some cases of non-intersection.
bool Intersects(const sreBoundingVolumeSphere& sphere, const sreBoundingVolumeConvexHull& ch);

bool Intersects(const sreBoundingVolumeCylinder& cyl, const sreBoundingVolumeConvexHull &ch);

bool Intersects(const sreBoundingVolumeHalfCylinder& hc, const sreBoundingVolumeConvexHull &ch);

// Intersection of a (convex or not) hull with vertex information with a convex hull.
bool Intersects(const sreBoundingVolumeHull& h1, const sreBoundingVolumeConvexHull& ch2);

// Intersection of a convex hull with vertex information with a convex hull.
static inline bool Intersects(const sreBoundingVolumeConvexHullWithVertices& ch1,
const sreBoundingVolumeConvexHull& ch2) {
     return Intersects(ch1.hull, ch2);
}

bool Intersects(const sreBoundingVolumeSphericalSector &spherical_sector,
const sreBoundingVolumeConvexHull &ch);

// Intersection of a convex hull with plane, center and plane radius information with another
// convex hull.
bool Intersects(const sreBoundingVolumeConvexHullFull& ch1, const sreBoundingVolumeConvexHull& ch2);

// Intersection of the infinite projection of a pyramid cone base with a frustum.
bool Intersects(const sreBoundingVolumeInfinitePyramidBase& pyramid, const sreBoundingVolumeFrustum& fr,
float cos_max_half_angular_size, float sin_max_half_angular_size);

// Intersection of the infinite projection of a spherical sector with a frustum.
bool Intersects(const sreBoundingVolumeInfiniteSphericalSector& spherical_sector,
const sreBoundingVolumeFrustum& fr, float cos_max_half_angular_size, float sin_max_half_angular_size);

bool IsCompletelyInside(const sreBoundingVolumeAABB& AABB1, const sreBoundingVolumeAABB& AABB2);

bool Intersects(const sreObject& so, const sreBoundingVolumeConvexHull& ch);

bool Intersects(const sreLight& light, const sreBoundingVolumeConvexHull& ch);

bool Intersects(const sreObject& so, const sreBoundingVolumeSphere& sphere);

bool Intersects(const sreObject& so, const sreLight& light);

BoundsCheckResult QueryIntersection(const sreObject& so, const sreLight& light);

BoundsCheckResult QueryIntersectionFull(const sreObject& so, const sreLight& light);

BoundsCheckResult QueryIntersectionFull(const sreObject& so, const sreLight& light,
    bool use_worst_case_bounds);

BoundsCheckResult QueryIntersection(const sreObject& so, const sreBoundingVolumeSphere& sphere);

BoundsCheckResult QueryIntersection(const sreOctreeNodeBounds& octree_bounds,
    const sreBoundingVolumeConvexHull& ch);

BoundsCheckResult QueryIntersection(const sreOctreeNodeBounds& octree_bounds,
    const sreBoundingVolumeSphere& sphere);

BoundsCheckResult QueryIntersection(const sreOctreeNodeBounds& octree_bounds, const sreLight& light);

bool IsCompletelyInside(const sreLight& light, const sreBoundingVolumeAABB& AABB);

bool Intersects(const sreObject& so, const sreBoundingVolumeFrustum& fr);

