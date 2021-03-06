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

#include <stdlib.h>
#include <stdio.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <limits.h>
#include <float.h>

#include "win32_compat.h"
#include "sre.h"
#include "sre_internal.h"
#include "sre_bounds.h"

//============================================================================
//
// Listing 16.7
//
// Mathematics for 3D Game Programming and Computer Graphics, 3rd ed.
// By Eric Lengyel
//
// The code in this file may be freely used in any software. It is provided
// as-is, with no warranty of any kind.
//
//============================================================================

static const float epsilon = 1.0e-10F;
static const int maxSweeps = 32;

static void CalculateEigensystem(const Matrix3D& m, float *lambda, Matrix3D& r)
{
	float m11 = m(0,0);
	float m12 = m(0,1);
	float m13 = m(0,2);
	float m22 = m(1,1);
	float m23 = m(1,2);
	float m33 = m(2,2);
	
	r.SetIdentity();
	for (int a = 0; a < maxSweeps; a++)
	{
		// Exit if off-diagonal entries small enough.
		if ((fabsf(m12) < epsilon) && (fabsf(m13) < epsilon) &&
			(fabsf(m23) < epsilon)) break;
		
		// Annihilate (1,2) entry.
		if (m12 != 0.0F)
		{
			float u = (m22 - m11) * 0.5F / m12;
			float u2 = u * u;
			float u2p1 = u2 + 1.0F;
			float t = (u2p1 != u2) ?
				((u < 0.0F) ? -1.0F : 1.0F) * (sqrtf(u2p1) - fabsf(u))
				: 0.5F / u;
			float c = 1.0F / sqrtf(t * t + 1.0F);
			float s = c * t;
			m11 -= t * m12;
			m22 += t * m12;
			m12 = 0.0F;
			
			float temp = c * m13 - s * m23;
			m23 = s * m13 + c * m23;
			m13 = temp;
			
			for (int i = 0; i < 3; i++)
			{
				float temp = c * r(i,0) - s * r(i,1);
				r(i,1) = s * r(i,0) + c * r(i,1);
				r(i,0) = temp;
			}
		}
		
		// Annihilate (1,3) entry.
		if (m13 != 0.0F)
		{
			float u = (m33 - m11) * 0.5F / m13;
			float u2 = u * u;
			float u2p1 = u2 + 1.0F;
			float t = (u2p1 != u2) ?
				((u < 0.0F) ? -1.0F : 1.0F) * (sqrtf(u2p1) - fabsf(u))
				: 0.5F / u;
			float c = 1.0F / sqrtf(t * t + 1.0F);
			float s = c * t;
			
			m11 -= t * m13;
			m33 += t * m13;
			m13 = 0.0F;
			
			float temp = c * m12 - s * m23;
			m23 = s * m12 + c * m23;
			m12 = temp;
			
			for (int i = 0; i < 3; i++)
			{
				float temp = c * r(i,0) - s * r(i,2);
				r(i,2) = s * r(i,0) + c * r(i,2);
				r(i,0) = temp;
			}
		}
		
		// Annihilate (2,3) entry.
		if (m23 != 0.0F)
		{
			float u = (m33 - m22) * 0.5F / m23;
			float u2 = u * u;
			float u2p1 = u2 + 1.0F;
			float t = (u2p1 != u2) ?
				((u < 0.0F) ? -1.0F : 1.0F) * (sqrtf(u2p1) - fabsf(u))
				: 0.5F / u;
			float c = 1.0F / sqrtf(t * t + 1.0F);
			float s = c * t;
			
			m22 -= t * m23;
			m33 += t * m23;
			m23 = 0.0F;
			
			float temp = c * m12 - s * m13;
			m13 = s * m12 + c * m13;
			m12 = temp;
			
			for (int i = 0; i < 3; i++)
			{
				float temp = c * r(i,1) - s * r(i,2);
				r(i,2) = s * r(i,1) + c * r(i,2);
				r(i,1) = temp;
			}
		}
	}
	
	lambda[0] = m11;
	lambda[1] = m22;
	lambda[2] = m33;
}

// sreBaseModel bounding volume calculation.

void sreBaseModel::CalculatePrincipalComponents(srePCAComponent *_PCA, Point3D &center) const {
    // Calculate the average position m.
    Point3D m;
    m.Set(0, 0, 0);
    for (int i = 0; i < nu_vertices; i++)
        m += position[i];
    m *= 1 / nu_vertices;

//    char *s = m.GetString();
//    sreMessage(SRE_MESSAGE_INFO, "Average position: %s", s);
//    delete s;

    // Calculate the covariance matrix.
    float C11, C22, C33, C12, C13, C23;
    C11 = 0;
    C22 = 0;
    C33 = 0;
    C12 = 0;
    C13 = 0;
    C23 = 0;
    for (int i = 0; i < nu_vertices; i++) {
        C11 += sqrf(position[i].x - m.x);
        C22 += sqrf(position[i].y - m.y);
        C33 += sqrf(position[i].z - m.z);
        C12 += (position[i].x - m.x) * (position[i].y - m.y);
        C13 += (position[i].x - m.x) * (position[i].z - m.z);
        C23 += (position[i].y - m.y) * (position[i].z - m.z);
    }
    C11 *= 1 / nu_vertices;
    C22 *= 1 / nu_vertices;
    C33 *= 1 / nu_vertices;
    C12 *= 1 / nu_vertices;
    C13 *= 1 / nu_vertices;
    C23 *= 1 / nu_vertices;
    Matrix3D C;
    C.Set(C11, C12, C13, C12, C22, C23, C13, C23, C33);
    // Calculate the eigenvectors.
    float lambda[3];
    Matrix3D r;
    CalculateEigensystem(C, lambda, r);
    _PCA[0].vector = r.GetRow(0).Normalize();
    _PCA[1].vector = r.GetRow(1).Normalize();
    _PCA[2].vector = r.GetRow(2).Normalize();
#if 0
    // Sort on decreasing lambda so that R is the eigenvector with the greatest lambda.
    // Note: this doesn't seem to guarantee ordering on size, we calculate the extents explicitly.
    if (lambda[0] < lambda[1]) {
        float temp = lambda[0];
        lambda[0] = lambda[1];
        lambda[1] = temp;
        Vector3D temp_V = _PCA[0].vector;
        _PCA[0].vector = _PCA[1].vector;
        _PCA[1].vector = temp_V;
    }
    if (lambda[1] < lambda[2]) {
        float temp = lambda[1];
        lambda[1] = lambda[2];
        lambda[2] = temp;
        Vector3D temp_V = _PCA[1].vector;
        _PCA[1].vector = _PCA[2].vector;
        _PCA[2].vector = temp_V;
    }
    if (lambda[0] < lambda[1]) {
        float temp = lambda[0];
        lambda[0] = lambda[1];
        lambda[1] = temp;
        Vector3D temp_V = _PCA[0].vector;
        _PCA[0].vector = _PCA[1].vector;
        _PCA[1].vector = temp_V;
    }
#endif
    // Given principal axis R, S, and T, calculate the minimum and maximum extents
    // in each direction.
    Vector3D PCA_C[3];
    float min_dot_product[3], max_dot_product[3];
    for (int i = 0; i < 3; i++) {
        PCA_C[i] = _PCA[i].vector;

//        char *s = PCA_C[i].GetString();
//	sreMessageNoNewline(SRE_MESSAGE_INFO, "PCA[%d] = %s ", i, s);
    }
//    sreMessage(SRE_MESSAGE_INFO, "");
    dstCalculateMinAndMaxDotProductNx3(nu_vertices, position,
        PCA_C, min_dot_product, max_dot_product);

//    sreMessageNoNewline(SRE_MESSAGE_INFO, "dstCalculateMinAndMaxDotProductNx3 (n = %d): ", nu_vertices);
//    for (int i = 0; i < 3; i++)
//        sreMessageNoNewline(SRE_MESSAGE_INFO, "min_dot[%d] = %f, max_dot[%d] = %f ", i,
//            min_dot_product[i], i, max_dot_product[i]);
//    sreMessage(SRE_MESSAGE_INFO, "");
 
    center.Set(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 3; i++) {
        _PCA[i].size = max_dot_product[i] - min_dot_product[i];
        center +=  (max_dot_product[i] + min_dot_product[i]) * 0.5f * _PCA[i].vector;
    }
    // Sort components on decreasing size so that R is the largest dimension.
    if (_PCA[0].size < _PCA[1].size) {
        float temp = _PCA[0].size;
        _PCA[0].size = _PCA[1].size;
        _PCA[1].size = temp;
        Vector3D temp_V = _PCA[0].vector;
        _PCA[0].vector = _PCA[1].vector;
        _PCA[1].vector = temp_V;
    }
    if (_PCA[1].size < _PCA[2].size) {
        float temp = _PCA[1].size;
        _PCA[1].size = _PCA[2].size;
        _PCA[2].size = temp;
        Vector3D temp_V = _PCA[1].vector;
        _PCA[1].vector = _PCA[2].vector;
        _PCA[2].vector = temp_V;
    }
    if (_PCA[0].size < _PCA[1].size) {
        float temp = _PCA[0].size;
        _PCA[0].size = _PCA[1].size;
        _PCA[1].size = temp;
        Vector3D temp_V = _PCA[0].vector;
        _PCA[0].vector = _PCA[1].vector;
        _PCA[1].vector = temp_V;
    }
}

void sreBaseModel::CalculatePCABoundingSphere(const srePCAComponent *_PCA,
sreBoundingVolumeSphere& sphere) const {
    // Given principal axis R, calculate the points Pk and Pl representing the minimum and
    // maximum extents in that direction.
    int i_Pmin, i_Pmax;
    dstGetIndicesWithMinAndMaxDotProductNx1(nu_vertices, (Vector3DPadded *)position, _PCA[0].vector,
        i_Pmin, i_Pmax);
//    sreMessage(SRE_MESSAGE_INFO, "i_Pmin = %d, i_Pmax = %d", i_Pmin, i_Pmax);

    // Calculate the center and radius.
    sphere.center = (vertex[i_Pmin] + vertex[i_Pmax]) * 0.5f;

#if 0
    char *s1 = vertex[i_Pmin].GetString();
    char *s2 = vertex[i_Pmax].GetString();
    char *s3 = sphere.center.GetString();
    char *PCA0_str = _PCA[0].vector.GetString();
    sreMessage(SRE_MESSAGE_INFO, "n = %d, PCA[0] = %s, V[i_Pmin] = %s, V[i_Pmax] = %s, sphere_center %s",
        nu_vertices, PCA0_str, s1, s2, s3);
    delete s1; delete s2; delete s3; delete PCA0_str;
#endif
    
    float r_squared = SquaredMag(vertex[i_Pmin] - sphere.center);
    // Make sure every point is inside the sphere.
    for (int i = 0; i < nu_vertices; i++) {
        float d_squared = SquaredMag(vertex[i] - sphere.center);
        if (d_squared > r_squared) {
            // Expand the sphere by placing the new center on the line connecting the previous center
            // and the point Pi. The new sphere is then tangent to the previous sphere at the point G.
            Point3D G = sphere.center - sqrtf(r_squared) * (vertex[i] - sphere.center) /
                Magnitude(vertex[i] - sphere.center);
            // The new center is placed halfway between the points G and Pi.
            sphere.center = (G + vertex[i]) * 0.5f;
            r_squared = SquaredMag(vertex[i] - sphere.center);
        }
    }
    sphere.radius = sqrtf(r_squared);
}

void sreBaseModel::CalculatePCABoundingEllipsoid(const srePCAComponent *_PCA,
sreBoundingVolumeEllipsoid& ellipsoid) const {
    sreBaseModel scaled_m;
    scaled_m.vertex = new Point3DPadded[nu_vertices];
    scaled_m.nu_vertices = nu_vertices;
    Matrix3D M_RST;
    M_RST.Set(_PCA[0].vector.x, _PCA[1].vector.x, _PCA[2].vector.x,
        _PCA[0].vector.y, _PCA[1].vector.y, _PCA[2].vector.y,
        _PCA[0].vector.z, _PCA[1].vector.z, _PCA[2].vector.z);
    Matrix3D M_scale;
    M_scale.Set(1.0f / _PCA[0].size, 0, 0, 0, 1.0 / _PCA[1].size, 0, 0, 0, 1.0f / _PCA[2].size);
    Matrix3D M = M_RST * (M_scale * Transpose(M_RST));
    for (int i = 0; i < nu_vertices; i++)
        scaled_m.vertex[i] = M * vertex[i];
    sreBoundingVolumeSphere scaled_sphere;
    scaled_m.CalculatePCABoundingSphere(_PCA, scaled_sphere);
    Matrix3D M_unscale;
    M_unscale.Set(_PCA[0].size, 0, 0, 0, _PCA[1].size, 0, 0, 0, _PCA[2].size);
    M = M_RST * (M_unscale * Transpose(M_RST));
    ellipsoid.center = M * scaled_sphere.center;
    ellipsoid.PCA[0].vector = _PCA[0].vector * _PCA[0].size * scaled_sphere.radius;
    ellipsoid.PCA[1].vector = _PCA[1].vector * _PCA[1].size * scaled_sphere.radius;
    ellipsoid.PCA[2].vector = _PCA[2].vector * _PCA[2].size * scaled_sphere.radius;
    delete [] scaled_m.vertex;
}

void sreBaseModel::CalculatePCABoundingCylinder(const srePCAComponent *_PCA,
sreBoundingVolumeCylinder& cylinder) const {
   Point3D *H = new Point3D[nu_vertices];
   for (int i = 0; i < nu_vertices; i++)
       H[i] = vertex[i] - Dot(vertex[i], _PCA[0].vector) * _PCA[0].vector;

    int i_Hmin, i_Hmax;
    dstGetIndicesWithMinAndMaxDotProductNx1(nu_vertices, (Vector3D *)H, _PCA[1].vector, i_Hmin, i_Hmax);

    cylinder.center = (H[i_Hmin] + H[i_Hmax]) * 0.5;
    float r_squared = SquaredMag(H[i_Hmin] - cylinder.center);
    for (int i = 0; i < nu_vertices; i++) {
        float d_squared = SquaredMag(H[i] - cylinder.center);
        if (d_squared > r_squared) {
            Point3D G = cylinder.center - sqrtf(r_squared) * (H[i] - cylinder.center)
                / Magnitude(H[i] - cylinder.center);
            cylinder.center = (G + H[i]) * 0.5;
            r_squared = SquaredMag(H[i] - cylinder.center);
        }
    }
    delete [] H;
    cylinder.radius = sqrtf(r_squared);
    cylinder.axis = _PCA[0].vector;
    cylinder.length = _PCA[0].size;
    float min_dot_product, max_dot_product;
    dstCalculateMinAndMaxDotProductNx1(nu_vertices, (Vector3DPadded *)vertex, _PCA[0].vector,
        min_dot_product, max_dot_product);
    cylinder.center = (min_dot_product + max_dot_product) * 0.5f * _PCA[0].vector;
    cylinder.CalculateAxisCoefficients();
}

void sreBaseModel::CalculateAABB(sreBoundingVolumeAABB& AABB) const {
    // Calculate the axis-aligned extents of the object.
    AABB.dim_min = Vector3D(POSITIVE_INFINITY_FLOAT, POSITIVE_INFINITY_FLOAT, POSITIVE_INFINITY_FLOAT);
    AABB.dim_max = Vector3D(NEGATIVE_INFINITY_FLOAT, NEGATIVE_INFINITY_FLOAT, NEGATIVE_INFINITY_FLOAT);
    for (int i = 0; i < nu_vertices; i++)
        UpdateAABB(AABB, vertex[i]);
}

// sreModel bounding volume calculation.

void sreModel::CalculateBoundingSphere() {
    sreBoundingVolumeSphere s;
    lod_model[0]->CalculatePCABoundingSphere(PCA, s);
    sphere = s;
}

void sreModel::CalculateBoundingBox() {
    if (sre_internal_debug_message_level >= 2)
        printf("Box center = (%f, %f, %f), %f x %f x %f\n", box_center.x, box_center.y, box_center.z,
            PCA[0].size, PCA[1].size, PCA[2].size);
}

void sreModel::CalculateBoundingEllipsoid(sreBoundingVolumeEllipsoid& ellipsoid) const {
    lod_model[0]->CalculatePCABoundingEllipsoid(PCA, ellipsoid);
}

void sreModel::CalculateBoundingCylinder(sreBoundingVolumeCylinder& cylinder) const {
    lod_model[0]->CalculatePCABoundingCylinder(PCA, cylinder);
}

void sreModel::CalculateAABB() {
    lod_model[0]->CalculateAABB(AABB);
}

#define EPSILON 0.00001
#define EPSILON2 0.0001

// Calculate bounding volume for the object. LOD model 0 is always used.
// Note: It would be better to calculate the bounds of all LOD models, and combine them
// so that the bounding volumes defined for the model are guaranteed to fit all LOD models.

void sreModel::CalculateBounds() {
    srePCAComponent _PCA[3];
    Point3D center;
    lod_model[0]->CalculatePrincipalComponents(_PCA, center);
    for (int i = 0; i < 3; i++)
        PCA[i] = _PCA[i];
    box_center = center;

    CalculateBoundingBox(); // Already calculated as PCA components and box_center.
    CalculateBoundingSphere();
    float volume_box = PCA[0].size * PCA[1].size * PCA[2].size;
    float volume_sphere = 4.0f / 3.0f * M_PI * sphere.radius * sphere.radius * sphere.radius;
    sreMessage(SRE_MESSAGE_LOG, "Bounding sphere: centre (%f, %f, %f), radius %f.",
        sphere.center.x, sphere.center.y, sphere.center.z, sphere.radius);
    if (volume_sphere > volume_box) {
        // Use the bounding sphere of the bounding box if it is smaller than the calculated bounding sphere.
        float sphere_box_radius = sqrtf(sqrf(PCA[0].size * 0.5) + sqrf(PCA[1].size * 0.5)
            + sqrf(PCA[2].size * 0.5));
        if (sphere_box_radius < sphere.radius) {
            sphere.center = box_center;
            sphere.radius = sphere_box_radius;
        sreMessage(SRE_MESSAGE_LOG,
            "Using bounding box for bounding sphere definition (radius = %f).", sphere.radius);
            volume_sphere = 4.0 / 3.0 * M_PI * sphere.radius * sphere.radius * sphere.radius;
        }
    }
    float best_volume;
    if (volume_box < volume_sphere) {
        if (PCA[0].size >= 4.0f * PCA[1].size)
            bounds_flags = SRE_BOUNDS_PREFER_BOX_LINE_SEGMENT;
        else
            bounds_flags = SRE_BOUNDS_PREFER_BOX;
        best_volume = volume_box;
    }
    else {
        bounds_flags = SRE_BOUNDS_PREFER_SPHERE;
        best_volume = volume_sphere;
    }
    // Calculate special bounding volumes and set it if it a good match.
    // Avoid calculating special bounding volumes for flat models (like ground). In practice the
    // computed PCA[2].size will rarely be exactly 0.0 even for completely flat models, so add a
    // small offset.
    if (PCA[2].size > EPSILON) {
        sreBoundingVolumeEllipsoid ellipsoid;
        CalculateBoundingEllipsoid(ellipsoid);
        float volume_ellipsoid = 4.0f / 3.0f * M_PI * Magnitude(ellipsoid.PCA[0].vector) *
            Magnitude(ellipsoid.PCA[1].vector) * Magnitude(ellipsoid.PCA[2].vector);
        sreMessage(SRE_MESSAGE_LOG, "Bounding ellipsoid volume %f, best volume %f.", volume_ellipsoid,
            best_volume);
        sreBoundingVolumeCylinder cylinder;
        CalculateBoundingCylinder(cylinder);
        float volume_cylinder = M_PI * sqrf(cylinder.radius) * cylinder.length;
        sreMessage(SRE_MESSAGE_LOG, "Bounding cylinder length = %f, radius = %f, volume = %f, "
            "best volume  = %f.", cylinder.length, cylinder.radius, volume_cylinder, best_volume);
        // Only use the ellipsoid when it is at least 1% better in volume, and impose a further
        // criterion on absolute the difference.
        if (volume_ellipsoid < 0.99f * best_volume && (best_volume - volume_ellipsoid) > EPSILON2
        && volume_ellipsoid <= volume_cylinder) {
            bounds_flags |= SRE_BOUNDS_PREFER_SPECIAL;
            bv_special.type = SRE_BOUNDING_VOLUME_ELLIPSOID;
            bv_special.ellipsoid = new sreBoundingVolumeEllipsoid;
            *bv_special.ellipsoid = ellipsoid;
            best_volume = volume_ellipsoid;
            sreMessage(SRE_MESSAGE_LOG,
                "Bounding ellipsoid provides smallest bounding volume of %f.",
                volume_ellipsoid);
        }
        else if (volume_cylinder < 0.99f * best_volume &&
        (best_volume - volume_cylinder) > EPSILON2) {
            bounds_flags |= SRE_BOUNDS_PREFER_SPECIAL;
            bv_special.type = SRE_BOUNDING_VOLUME_CYLINDER;
            bv_special.cylinder = new sreBoundingVolumeCylinder;
            *bv_special.cylinder = cylinder;
            best_volume = volume_cylinder;
            sreMessage(SRE_MESSAGE_LOG,
                "Bounding cylinder provides smallest bounding volume of %f.",
                volume_cylinder);
        }
    }
    CalculateAABB();
    float AABB_volume = (AABB.dim_max.x - AABB.dim_min.x) * (AABB.dim_max.y - AABB.dim_min.y)
        * (AABB.dim_max.z - AABB.dim_min.z);
    if (0.99f * AABB_volume <= volume_box)
        bounds_flags |= SRE_BOUNDS_PREFER_AABB;
    if (sre_internal_debug_message_level >= 2) {
        const char *basic;
        if (bounds_flags & SRE_BOUNDS_PREFER_BOX_LINE_SEGMENT)
            basic = "Box (line segment test)";
        else if (bounds_flags & SRE_BOUNDS_PREFER_BOX)
            basic = "Box (box test)";
        else
            basic = "Sphere";
        const char *aabb;
        if (bounds_flags & SRE_BOUNDS_PREFER_AABB)
            aabb = " (PREFER_AABB is set for box)";
        else
            aabb = "";
        const char *special;
        if (bounds_flags & SRE_BOUNDS_PREFER_SPECIAL) {
            if (bv_special.type == SRE_BOUNDING_VOLUME_ELLIPSOID)
                special = "Ellipsoid";
            else
                special = "Cylinder";
        }
        else
            special = "None";
        sreMessage(SRE_MESSAGE_LOG, "Bounding volume selected: basic: %s%s, special: %s", basic, aabb,
            special);
    }
}

void sreModel::SetOBBWithAABBBounds(const sreBoundingVolumeAABB& _AABB) {
    PCA[0].vector.Set(1, 0, 0);
    PCA[1].vector.Set(0, 1, 0);
    PCA[2].vector.Set(0, 0, 1);
    box_center.Set(
        (_AABB.dim_max.x + _AABB.dim_min.x) * 0.5f,
        (_AABB.dim_max.y + _AABB.dim_min.y) * 0.5f,
        (_AABB.dim_max.z + _AABB.dim_min.z) * 0.5f);
    float box_dimensions_R = _AABB.dim_max.x - _AABB.dim_min.x;
    float box_dimensions_S = _AABB.dim_max.y - _AABB.dim_min.y;
    float box_dimensions_T = _AABB.dim_max.z - _AABB.dim_min.z;
    PCA[0].size = box_dimensions_R;
    PCA[1].size = box_dimensions_S;
    PCA[2].size = box_dimensions_T;
    bounds_flags = SRE_BOUNDS_PREFER_BOX;
}

void sreModel::SetBoundingCollisionShapeCapsule(const sreBoundingVolumeCapsule& capsule) {
    special_collision_shape = new sreBoundingVolume;
    special_collision_shape->capsule = new sreBoundingVolumeCapsule;
    special_collision_shape->type = SRE_BOUNDING_VOLUME_CAPSULE;
    *special_collision_shape->capsule = capsule;
    bounds_flags |= SRE_BOUNDS_SPECIAL_SRE_COLLISION_SHAPE;
}

// Get the maximum extents of a model. All LOD models are checked.

void sreModel::GetMaxExtents(sreBoundingVolumeAABB *AABB_out, float *max_dim_out) {
    sreBoundingVolumeAABB aabb;
    lod_model[0]->CalculateAABB(aabb);
    for (int i = 1; i < nu_lod_levels; i++) {
        sreBoundingVolumeAABB lod_aabb;
        lod_model[i]->CalculateAABB(lod_aabb);
        UpdateAABB(aabb, lod_aabb);
    }
    float max_dim = max3f(
        aabb.dim_max.x - aabb.dim_min.x,
        aabb.dim_max.y - aabb.dim_min.y,
        aabb.dim_max.z - aabb.dim_min.z);
    if (AABB_out != NULL)
        *AABB_out = aabb;
    if (max_dim_out != NULL)
        *max_dim_out = max_dim;
}
