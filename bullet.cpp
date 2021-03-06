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
#include <math.h>
#ifdef __GNUC__
#include <fenv.h>
#endif
#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionShapes/btShapeHull.h>

#include "sre.h"
#include "sreBackend.h"

btBroadphaseInterface* broadphase;
btDefaultCollisionConfiguration* collisionConfiguration;
btCollisionDispatcher* dispatcher;
btSequentialImpulseConstraintSolver* solver;
btDiscreteDynamicsWorld* dynamicsWorld;
btRigidBody** object_rigid_body;
btRigidBody* ground_rigid_body;
sreScene *sre_bullet_internal_scene;

class MyMotionState : public btMotionState {
protected :
    int mSoi;
    btTransform mPos1;

public :
    MyMotionState(const btTransform& initialpos, int so_index) {
        mSoi = so_index;
        mPos1 = initialpos;
    }

    virtual ~MyMotionState() {
    }

    void setsreObject(int so_index) {
        mSoi = so_index;
    }

    virtual void getWorldTransform(btTransform& worldTrans) const {
        worldTrans = mPos1;
    }

    virtual void setWorldTransform(const btTransform &worldTrans) {
        btMatrix3x3 rot = worldTrans.getBasis();
        btVector3 row0 = rot.getRow(0);
        btVector3 row1 = rot.getRow(1);
        btVector3 row2 = rot.getRow(2);
        Matrix3D rot2;
        rot2.Set(row0.x(), row0.y(), row0.z(), row1.x(), row1.y(), row1.z(),
            row2.x(), row2.y(), row2.z());
        btVector3 pos = worldTrans.getOrigin();
        sreObject *so = sre_bullet_internal_scene->object[mSoi];
        Point3D position;
        position.Set(pos.x(), pos.y(), pos.z());
        position -= rot2 * so->collision_shape_center_offset;
        sre_bullet_internal_scene->ChangePositionAndRotationMatrix(mSoi, position.x, position.y, position.z, rot2);
//        printf("Changing position of object %d to (%lf, %lf, %lf).\n",
//            mSoi, pos.x(), pos.y(), pos.z());
    }

    void setKinematicPosition(const btTransform& transform) {
        mPos1 = transform;
    }
};

class CollisionShapeInfo {
public:
    btCollisionShape *shape;
    sreObject *so;
};

static bool IsSameScale(sreObject *so1, sreObject *so2) {
    if (so1->scaling != so2->scaling)
        return false;
    return true;
}

void sreBulletPhysicsApplication::InitializePhysics() {
    sreMessage(SRE_MESSAGE_INFO, "Creating bullet data structures.");

    sre_bullet_internal_scene = scene;

    // Build the broadphase
    broadphase = new btDbvtBroadphase();

    // Set up the collision configuration and dispatcher
    collisionConfiguration = new btDefaultCollisionConfiguration();
    dispatcher = new btCollisionDispatcher(collisionConfiguration);

    // The actual physics solver
    solver = new btSequentialImpulseConstraintSolver;

    // The world.
    dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collisionConfiguration);
    dynamicsWorld->setGravity(btVector3(0, 0, - 20.0f));

    // Add the ground.
    if (!(flags & SRE_APPLICATION_FLAG_NO_GROUND_PLANE)) {
        btCollisionShape* groundShape = new btStaticPlaneShape(btVector3(0, 0, 1.0), 0);
        btDefaultMotionState* groundMotionState = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1),
            btVector3(0, 0, 0)));
        btRigidBody::btRigidBodyConstructionInfo groundRigidBodyCI(0, groundMotionState, groundShape, btVector3(0, 0, 0));
        groundRigidBodyCI.m_restitution = 0;
        groundRigidBodyCI.m_friction  = 1;
        ground_rigid_body = new btRigidBody(groundRigidBodyCI);
        dynamicsWorld->addRigidBody(ground_rigid_body);
    }

    if (scene->nu_objects == 0)
        return;

    // Rigid bodies for world objects.
    object_rigid_body = new btRigidBody *[scene->nu_objects];

    // First pass over the other objects: calculate necessary collision shapes.
    // Cache the shape of the last instance of a model object so that the same bullet shape will be used if consecutive
    // scene objects have the same dimensions.
    CollisionShapeInfo *static_object_collision_shape_info = (CollisionShapeInfo *)alloca(sizeof(CollisionShapeInfo) * scene->models.Size());
    CollisionShapeInfo *dynamic_object_collision_shape_info = (CollisionShapeInfo *)alloca(sizeof(CollisionShapeInfo) *scene->models.Size());
    for (int i = 0; i < scene->models.Size(); i++) {
        static_object_collision_shape_info[i].shape = NULL;
        dynamic_object_collision_shape_info[i].shape = NULL;
    }
    btCollisionShape **scene_object_collision_shape = (btCollisionShape **)alloca(sizeof(btCollisionShape *) * scene->nu_objects);
    bool *collision_shape_is_static = (bool *)alloca(sizeof(bool) * scene->nu_objects);
    bool *collision_shape_is_absolute = (bool *)alloca(sizeof(bool) * scene->nu_objects);
    for (int i = 0; i < scene->nu_objects; i++) {
        sreObject *so = scene->object[i];
        if (so->flags & SRE_OBJECT_NO_PHYSICS)
            continue;
        // Pick LOD level 0. Chosing a lower detail LOD level may help performance.
        sreLODModel *m = so->model->lod_model[so->physics_lod_level];
        int collision_shape_type;
        if (so->flags & SRE_OBJECT_DYNAMIC_POSITION)
            collision_shape_type = so->model->collision_shape_dynamic;
        else {
            collision_shape_type = so->model->collision_shape_static;
        }
        collision_shape_is_static[i] = false;
        collision_shape_is_absolute[i] = false;
        so->collision_shape_center_offset.Set(0, 0, 0);
        if (collision_shape_type == SRE_COLLISION_SHAPE_STATIC) {
            // Static mesh.
            btTriangleMesh *mTriMesh = new btTriangleMesh();
            // Add unoptimized unindexed triangle.
            for (int j = 0; j < m->nu_triangles; j++) {
                Point3D V0 = (so->model_matrix * m->vertex[m->triangle[j].vertex_index[0]]).GetPoint3D();
                Point3D V1 = (so->model_matrix * m->vertex[m->triangle[j].vertex_index[1]]).GetPoint3D();
                Point3D V2 = (so->model_matrix * m->vertex[m->triangle[j].vertex_index[2]]).GetPoint3D();
                btVector3 W0(V0.x, V0.y, V0.z);
                btVector3 W1(V1.x, V1.y, V1.z);
                btVector3 W2(V2.x, V2.y, V2.z);
                mTriMesh->addTriangle(W0, W1, W2);
            }
            scene_object_collision_shape[i] = new btBvhTriangleMeshShape(mTriMesh, true);
            collision_shape_is_static[i] = true;
            collision_shape_is_absolute[i] = true;
            continue;
        }
        // Calculate the correction displacement to Bullet's origin for the shape.
        // For some shapes rotation has to be applied so that the bounding volume center
        // (which is rotated) matches the Bullet collision shape before rotation.
        switch (collision_shape_type) {
        case SRE_COLLISION_SHAPE_SPHERE :
            // This will normally be equal to the zero vector.
            so->collision_shape_center_offset = so->sphere.center - so->position;
//            so->collision_shape_center_offset = (so->rotation_matrix * so->model->sphere.center) *
//               so->scaling;
            break;
        case SRE_COLLISION_SHAPE_BOX :
              so->collision_shape_center_offset = so->box.center - so->position;
//            so->collision_shape_center_offset = (so->rotation_matrix * so->model->box_center) *
//                so->scaling;
            break;
        case SRE_COLLISION_SHAPE_CYLINDER :
            // The origin of the cylinder model is the center of the bottom of the cylinder. Calculate
            // the correct adjustment for the collision shape's geometrical center in the middle of the
            // cylinder.
            so->collision_shape_center_offset = 0.5f * so->bv_special.cylinder->axis *
                so->bv_special.cylinder->length;
            break;
        case SRE_COLLISION_SHAPE_ELLIPSOID :
              so->collision_shape_center_offset = so->bv_special.ellipsoid->center - so->position;
//            so->collision_shape_center_offset = (so->rotation_matrix *
//               so->model->bv_special.ellipsoid->center) * so->scaling;
            break;
        case SRE_COLLISION_SHAPE_CAPSULE :
            // No adjustment should be necessary, a capsule shape implies a center at the origin
            // in model space.
            break;
        }
        int id = so->model->id;
        if (!(so->flags & SRE_OBJECT_DYNAMIC_POSITION))
            collision_shape_is_static[i] = true;
        // Check to see whether the shape is in the cache.
        if (!(so->flags & SRE_OBJECT_DYNAMIC_POSITION) &&
        static_object_collision_shape_info[id].shape != NULL &&
        IsSameScale(static_object_collision_shape_info[id].so, so)) {
            scene_object_collision_shape[i] = static_object_collision_shape_info[id].shape;
            continue;
        }
        if ((so->flags & SRE_OBJECT_DYNAMIC_POSITION) &&
        dynamic_object_collision_shape_info[id].shape != NULL &&
        IsSameScale(dynamic_object_collision_shape_info[id].so, so)) {
            scene_object_collision_shape[i] = dynamic_object_collision_shape_info[id].shape;
            continue;
        }
        switch (collision_shape_type) {
        case SRE_COLLISION_SHAPE_SPHERE :
            scene_object_collision_shape[i] = new btSphereShape(so->sphere.radius);
            break;
        case SRE_COLLISION_SHAPE_BOX : {
            // Assumes axis-aligned object space PCA components.
            srePCAComponent PCA[3];
            if (so->model->is_static) {
                // If the object was converted to static scenery, the PCA directions rotated so we have
                // to convert them back.
                Matrix3D inverted_rotation_matrix = Inverse(*so->original_rotation_matrix);
                PCA[0].vector = inverted_rotation_matrix * so->model->PCA[0].vector;
                PCA[1].vector = inverted_rotation_matrix * so->model->PCA[1].vector;
                PCA[2].vector = inverted_rotation_matrix * so->model->PCA[2].vector;         
            }
            else {
                PCA[0].vector = so->model->PCA[0].vector;
                PCA[1].vector = so->model->PCA[1].vector;
                PCA[2].vector = so->model->PCA[2].vector;         
            }
            float dimx, dimy, dimz;
            if (fabsf(PCA[0].vector.x) > 0.5)
                dimx = so->model->PCA[0].size * so->scaling;
            else if (fabsf(PCA[1].vector.x) > 0.5)
                dimx = so->model->PCA[1].size * so->scaling;
            else
                dimx = so->model->PCA[2].size * so->scaling;
            if (fabsf(PCA[0].vector.y) > 0.5)
                dimy = so->model->PCA[0].size * so->scaling;
            else if (fabsf(PCA[1].vector.y) > 0.5)
                dimy = so->model->PCA[1].size * so->scaling;
            else
                dimy = so->model->PCA[2].size * so->scaling;
            if (fabsf(PCA[0].vector.z) > 0.5)
                dimz = so->model->PCA[0].size * so->scaling;
            else if (fabsf(PCA[1].vector.z) > 0.5)
                dimz = so->model->PCA[1].size * so->scaling;
            else
                dimz = so->model->PCA[2].size * so->scaling;
            scene_object_collision_shape[i] = new btBoxShape(btVector3(dimx * 0.5, dimy * 0.5, dimz * 0.5));
            break;
            }
        case SRE_COLLISION_SHAPE_CYLINDER :
            // Assumes the length is defined as the z-axis.
            scene_object_collision_shape[i] = new btCylinderShapeZ(
                btVector3(so->bv_special.cylinder->radius,
                so->bv_special.cylinder->radius, so->bv_special.cylinder->length * 0.5f));
            break;
        case SRE_COLLISION_SHAPE_CONVEX_HULL : {
            btConvexHullShape *mConvexHull = new btConvexHullShape();
            // Add unoptimized unindexed triangle.
            for (int j = 0; j < m->nu_vertices; j++) {
                Point3D V0 = m->vertex[j] * so->scaling;
                btVector3 W0(V0.x, V0.y, V0.z);
                mConvexHull->addPoint(W0);
            }
	    // Create a hull approximation.
            btShapeHull* hull = new btShapeHull(mConvexHull);
            btScalar margin = mConvexHull->getMargin();
            hull->buildHull(margin);
            scene_object_collision_shape[i] = new btConvexHullShape(
                (const btScalar*)hull->getVertexPointer(),
                hull->numVertices());
            sreMessage(SRE_MESSAGE_SPARSE_LOG,
                "Convex hull vertices reduced from %d to %d.\n",
                m->nu_vertices, hull->numVertices());
            break;
            }
        case SRE_COLLISION_SHAPE_ELLIPSOID : {
            // Assumes the largest axis is aligned with the x-axis, the second largest with y
            // and the smallest with z.
            btVector3 positions[1];
            btScalar radii[1];
            positions[0] = btVector3(0, 0, 0);
            radii[0] = Magnitude(so->bv_special.ellipsoid->PCA[0].vector);
            scene_object_collision_shape[i] = new btMultiSphereShape(positions, radii, 1);
            scene_object_collision_shape[i]->setLocalScaling(btVector3(1.0,
                Magnitude(so->bv_special.ellipsoid->PCA[1].vector) / radii[0],
                Magnitude(so->bv_special.ellipsoid->PCA[2].vector) / radii[0]));
            break;
            }
        case SRE_COLLISION_SHAPE_CAPSULE :
            // Assumes the length is defined as the x-axis.
            scene_object_collision_shape[i] = new btCapsuleShapeX(
                so->model->special_collision_shape->capsule->radius *
                so->scaling, so->model->special_collision_shape->capsule->length * so->scaling);
            scene_object_collision_shape[i]->setLocalScaling(btVector3(1.0,
                so->model->special_collision_shape->capsule->radius_y,
                so->model->special_collision_shape->capsule->radius_z));
            break;
        }
        // Put the shape in the "cache" of the most recently used scaling for the model.
        if (!(so->flags & SRE_OBJECT_DYNAMIC_POSITION)) {
            static_object_collision_shape_info[id].shape = scene_object_collision_shape[i];
            static_object_collision_shape_info[id].so = so;
        }
        else {
            dynamic_object_collision_shape_info[id].shape = scene_object_collision_shape[i];
            dynamic_object_collision_shape_info[id].so = so;
        }
    }

    // Add the objects to the collision world.
    for (int i = 0; i < scene->nu_objects; i++) {
        sreObject *so = scene->object[i];
        if (so->flags & SRE_OBJECT_NO_PHYSICS)
            continue;
        // Static object instantiation. Generally physics objects with the
        // SRE_OBJECT_DYNAMIC_POSITION flag set are considered dynamic, others static.
        // When this flag is set, the mass parameter will be passed to Bullet.
        if (collision_shape_is_static[i] && collision_shape_is_absolute[i]) {
            // Fixed static object with mesh with absolute coordinates.
            btMotionState* meshMotionState;
            meshMotionState = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, 0, 0)));
            btRigidBody::btRigidBodyConstructionInfo meshRigidBodyCI(0, meshMotionState,
                scene_object_collision_shape[i], btVector3(0, 0, 0));
            meshRigidBodyCI.m_restitution = 0;
            meshRigidBodyCI.m_friction  = 1;
            btRigidBody* meshRigidBody = new btRigidBody(meshRigidBodyCI);
            dynamicsWorld->addRigidBody(meshRigidBody);
        }
        else if (!collision_shape_is_absolute[i]) {
            // The collision shape is a geometric shape (box, sphere, cylinder etc) with a local
            // coordinate system.
            // Calculate the object position for Bullet, reflecting the center of the local coordinate
            // system of the collision shape.
            Point3D pos = so->position + so->collision_shape_center_offset;
            btMatrix3x3 rot;
            // Set the rotation matrix for Bullet.
            // If the object was converted to static absolute scenery in the preprocessing stage,
            // so that the rotation matrix for the object was set to identity, recover the original
            // rotation matrix.
            if (so->model->is_static) {
		Vector3D row0, row1, row2;
		row0 = so->original_rotation_matrix->GetRow(0);
		row1 = so->original_rotation_matrix->GetRow(1);
		row2 = so->original_rotation_matrix->GetRow(2);
                rot[0] = btVector3(row0.x, row0.y, row0.z);
                rot[1] = btVector3(row1.x, row1.y, row1.z);
                rot[2] = btVector3(row2.x, row2.y, row2.z);
            }
            else {
		Vector3D row0, row1, row2;
		row0 = so->rotation_matrix.GetRow(0);
		row1 = so->rotation_matrix.GetRow(1);
		row2 = so->rotation_matrix.GetRow(2);
                rot[0] = btVector3(row0.x, row0.y, row0.z);
                rot[1] = btVector3(row1.x, row1.y, row1.z);
                rot[2] = btVector3(row2.x, row2.y, row2.z);
            }
            if (collision_shape_is_static[i]) {
                // Static object instantiation.
                btMotionState* meshMotionState;
                meshMotionState = new btDefaultMotionState(btTransform(rot, btVector3(pos.x, pos.y, pos.z)));
                btRigidBody::btRigidBodyConstructionInfo meshRigidBodyCI(0, meshMotionState,
                   scene_object_collision_shape[i], btVector3(0, 0, 0));
                meshRigidBodyCI.m_restitution = 0;
                meshRigidBodyCI.m_friction = 1.0f;
                btRigidBody* meshRigidBody = new btRigidBody(meshRigidBodyCI);
                dynamicsWorld->addRigidBody(meshRigidBody);
            }
            else {
                // Dynamic object.
                MyMotionState* objectMotionState = new MyMotionState(
                    btTransform(rot, btVector3(pos.x, pos.y, pos.z)), i);
                btScalar mass = so->mass;
                if (so->flags & SRE_OBJECT_KINEMATIC_BODY)
                    mass = 0;
                btVector3 objectInertia(0, 0, 0);
                // Following line belongs in pass one?
                scene_object_collision_shape[i]->calculateLocalInertia(mass, objectInertia);
                btRigidBody::btRigidBodyConstructionInfo objectRigidBodyCI(mass, objectMotionState,
                    scene_object_collision_shape[i], objectInertia);
                objectRigidBodyCI.m_restitution = 0;
                objectRigidBodyCI.m_friction = 1;
                objectRigidBodyCI.m_angularDamping = 0.5;
                object_rigid_body[i] = new btRigidBody(objectRigidBodyCI);
                if (so->flags & SRE_OBJECT_KINEMATIC_BODY)
                    object_rigid_body[i]->setCollisionFlags(object_rigid_body[i]->getCollisionFlags() |
                        btCollisionObject::CF_KINEMATIC_OBJECT |
                        btCollisionObject::CF_NO_CONTACT_RESPONSE);
                // Activate all objects at start-up. Objects will be disactivated after a few 
                // seconds. However, when the player moves it will always be activated, which
                // should trigger activation for any objects that are interacted with.
                object_rigid_body[i]->activate(false);
                // Disabling deactivation is unnecessary.
//                object_rigid_body[i]->setActivationState(DISABLE_DEACTIVATION);
                dynamicsWorld->addRigidBody(object_rigid_body[i]);
            }
        }
        else {
            // Absolute and not static makes no sense.
            sreFatalError("Error in BulletInitialize.\n");
        }
    }
}

void sreBulletPhysicsApplication::DestroyPhysics() {
    sreMessage(SRE_MESSAGE_INFO, "Deleting physics data structures.");
    delete dynamicsWorld;
    delete solver;
    delete dispatcher;
    delete collisionConfiguration;
    delete broadphase;
}

static void BulletStep(double dt) {
    int substeps = 5;
    if (dt >= ((double)substeps) / (double)60) {
#ifdef __GNUC__
        fesetround(FE_DOWNWARD);
        substeps = lrint(dt / ((double)1 / 60)) + 1;
#else
        substeps = floor(dt / ((double)1 / 60)) + 1;
#endif
        sreMessage(SRE_MESSAGE_LOG,
            "Substeps adjusted to %d, dt = %lf, substeps * 1 / 60 = %lf\n",
            substeps, dt, substeps * ((double)1 / 60));
    }
    dynamicsWorld->stepSimulation(dt, substeps);
}

void sreBulletPhysicsApplication::DoPhysics(double previous_time, double current_time) {
    Vector3D gravity;
    sreMovementMode movement_mode = view->GetMovementMode();
    if ((flags & SRE_APPLICATION_FLAG_DYNAMIC_GRAVITY) &&
    movement_mode != SRE_MOVEMENT_MODE_NONE) {
        btVector3 pos = object_rigid_body[control_object]->getCenterOfMassPosition();
        gravity = Vector3D(gravity_position.x - pos.x(), gravity_position.y - pos.y(), gravity_position.z - pos.z());
        gravity.Normalize();
        gravity *= 20.0;
    }
    double dt = current_time - previous_time;
    // When user movement is disabled, don't alter any object manually.
    if (movement_mode == SRE_MOVEMENT_MODE_NONE || control_object < 0) {
        BulletStep(dt);
        return;
    }
    if ((flags & SRE_APPLICATION_FLAG_JUMP_ALLOWED) && jump_requested) {
        btVector3 delta(0, 0, 30.0);
        if (flags & SRE_APPLICATION_FLAG_DYNAMIC_GRAVITY) {
            delta = btVector3(- gravity.x * 1.5, - gravity.y * 1.5, - gravity.z * 1.5);
        }
        object_rigid_body[control_object]->activate(false);
        object_rigid_body[control_object]->applyCentralImpulse(delta);
        jump_requested = false;
    }
    Vector3D ascend;
    if (movement_mode == SRE_MOVEMENT_MODE_USE_FORWARD_AND_ASCEND_VECTOR)
       ascend = view->GetAscendVector();
    else
       ascend = Vector3D(0, 0, 1.0f);
    if (input_acceleration != 0) {
        // When there is control input, make sure the control object is activated in Bullet.
        object_rigid_body[control_object]->activate(false);
        Vector3D input_velocity;
        if (input_acceleration < 0) {
            // When decelerating, reduce the existing horizontal velocity.
            Vector3D vel;
            scene->BulletGetLinearVelocity(control_object, &vel);
            Vector3D vertical_velocity = ProjectOnto(vel, ascend);
            Vector3D horizontal_velocity = vel - vertical_velocity;
            float mag = Magnitude(horizontal_velocity);
            Vector3D horizontal_velocity_direction = horizontal_velocity / mag;
            if (mag > 0)
                input_velocity = input_acceleration * horizontal_velocity_direction;
            else
                input_velocity = Vector3D(0, 0, 0);
        }
        else
        if (movement_mode == SRE_MOVEMENT_MODE_USE_FORWARD_AND_ASCEND_VECTOR) {
            input_velocity = view->GetForwardVector() * input_acceleration;
        }
        else {
            Vector3D angles;
            view->GetViewAngles(angles);
            Matrix4D m;
            m.AssignRotationAlongZAxis(- angles.z * M_PI / 180.0f);
            Vector4D v(0, 1.0, 0, 1.0f);
            input_velocity = (v * m).GetVector3D() * input_acceleration;
        }
        Vector3D vel;
        btVector3 delta(input_velocity.x, input_velocity.y, input_velocity.z);
        object_rigid_body[control_object]->applyCentralImpulse(delta);
        scene->BulletGetLinearVelocity(control_object, &vel);
        // Limit max velocity in horizontal movement plane.
        Vector3D vertical_velocity = ProjectOnto(vel, ascend);
        Vector3D horizontal_velocity = vel - vertical_velocity;
        float mag = Magnitude(horizontal_velocity);
        if (mag > max_horizontal_velocity) {
            vel = horizontal_velocity * max_horizontal_velocity / mag + vertical_velocity;
            scene->BulletChangeVelocity(control_object, vel);
        }
        input_acceleration = 0;
    }
    if (flags & SRE_APPLICATION_FLAG_NO_GRAVITY) {
        // Move control object vertically to hovering height.
        object_rigid_body[control_object]->activate(false); // Activate the control object.
        object_rigid_body[control_object]->setGravity(btVector3(0, 0, 0));
        btVector3 posb = object_rigid_body[control_object]->getCenterOfMassPosition();
        Point3D pos = Point3D(posb.x(), posb.y(), posb.z());
        float height;
        if (movement_mode == SRE_MOVEMENT_MODE_USE_FORWARD_AND_ASCEND_VECTOR) {
            height = Magnitude(ProjectOnto(scene->object[control_object]->position,
                view->GetAscendVector()));
        }
        else {
            height = pos.z;
        }
        Vector3D delta = Vector3D(0, 0, 0);
        Vector3D vel;
        scene->BulletGetLinearVelocity(control_object, &vel);
        Vector3D vertical_velocity = ProjectOnto(vel, ascend);
        Vector3D rem = vel - vertical_velocity;
        if (height < hovering_height - 1.0f)
            delta = ascend * pow(hovering_height - height, 1.5f) * dt * 20.0f;
        else
        if (height > hovering_height + 1.0f)
            delta = - ascend * pow(height - hovering_height, 1.5f) * dt * 20.0f;
        object_rigid_body[control_object]->setLinearVelocity(btVector3(rem.x,
            rem.y, rem.z));
        rem += delta;
        scene->BulletChangePosition(control_object, pos + rem);
        scene->ChangePosition(control_object, pos + rem);
        object_rigid_body[control_object]->applyCentralImpulse(btVector3(delta.x, delta.y, delta.z));
    }
    else if (flags & SRE_APPLICATION_FLAG_DYNAMIC_GRAVITY) {
        object_rigid_body[control_object]->setGravity(btVector3(gravity.x, gravity.y, gravity.z));
    }
    else {
        object_rigid_body[control_object]->setGravity(btVector3(0, 0, - 20.0f));
    }

    BulletStep(dt);
}

void sreScene::BulletApplyCentralImpulse(int soi, const Vector3D& v) const {
    // Activate the object.
    object_rigid_body[soi]->activate(false);
    btVector3 delta(v.x, v.y, v.z);
    // Apply impulse.
    object_rigid_body[soi]->applyCentralImpulse(delta);
}

void sreScene::BulletGetLinearVelocity(int soi, Vector3D *v_out) const {
    btVector3 bv = object_rigid_body[soi]->getLinearVelocity();
    v_out->x = bv.x();
    v_out->y = bv.y();
    v_out->z = bv.z();
}

void sreScene::BulletChangePosition(int soi, Point3D position) const {
    if (object[soi]->flags & SRE_OBJECT_KINEMATIC_BODY) {
        MyMotionState *motion_state = (MyMotionState *)object_rigid_body[soi]->getMotionState();
        btTransform world_transform;
        motion_state->getWorldTransform(world_transform);
        world_transform.setOrigin(btVector3(position.x, position.y, position.z));
        motion_state->setKinematicPosition(world_transform);
        return;
    }
    btVector3 current_pos = object_rigid_body[soi]->getCenterOfMassPosition();
    Vector3D delta = Vector3D(position.x - current_pos.x(), position.y - current_pos.y(), position.z - current_pos.z());
    object_rigid_body[soi]->activate(true);
    object_rigid_body[soi]->translate(btVector3(delta.x, delta.y, delta.z));
}

void sreScene::BulletChangeVelocity(int soi, Vector3D velocity) const {
    // Activate the object.
    object_rigid_body[soi]->activate(false);
    object_rigid_body[soi]->setLinearVelocity(btVector3(velocity.x, velocity.y, velocity.z));
}

void sreScene::BulletChangeRotationMatrix(int soi, const Matrix3D& rot_matrix) const {
    btTransform world_transform;
    MyMotionState *motion_state;
    if (object[soi]->flags & SRE_OBJECT_KINEMATIC_BODY) {
        motion_state = (MyMotionState *)object_rigid_body[soi]->getMotionState();
        motion_state->getWorldTransform(world_transform);
    }
    else
        world_transform = object_rigid_body[soi]->getWorldTransform();
    Vector3D row0, row1, row2;
    row0 = rot_matrix.GetRow(0);
    row1 = rot_matrix.GetRow(1);
    row2 = rot_matrix.GetRow(2);
    btMatrix3x3 basis = btMatrix3x3(row0.x, row0.y, row0.z, row1.x, row1.y, row1.z,
        row2.x, row2.y, row2.z);
    world_transform.setBasis(basis);
    if (object[soi]->flags & SRE_OBJECT_KINEMATIC_BODY)
        motion_state->setKinematicPosition(world_transform);
    else {
        object_rigid_body[soi]->activate(true);
        object_rigid_body[soi]->setWorldTransform(world_transform);
    }
}

