#include "objects.h"

#include <iostream>
#include <cmath>

bool Sphere::IntersectRay( Ray const &ray, HitInfo &hInfo, int hitSide ) const
{
    float bias = 0.002;
    bool front = true;

    // q is [0, 0, 0] and r = 1
    float a = ray.dir.Dot(ray.dir);
    float b = 2 * ray.dir.Dot(ray.p);
    float c = ray.p.Dot(ray.p) - 1;

    float delta = (b * b) - (4 * a * c);
    if (delta < 0) return false;

    float t = ( -b - sqrt(delta) ) / ( 2 * a );
    if ( t <= bias ) {
        if (hitSide == HIT_BACK || hitSide == HIT_FRONT_AND_BACK) {
            front = false;
            t = ( -b + sqrt(delta) ) / ( 2 * a );
            if (t <= bias) {
                return false;
            }
        }
        else {
            return false;
        }
    }

    //okay so we actually hit, so let's check if it's closer than previous stored hit
    cy::Vec3f p = ray.p + ray.dir * t;

    if ( abs(p.Dot(ray.dir)) <= bias) return false; //too close to parallel??

    if (t < hInfo.z) {
        hInfo.z = t;
        hInfo.front = front;
        hInfo.p = p;
        hInfo.N = p;    // we'll normalize it later since we have to apply transforms anyway
        hInfo.GN = p;
        // calculate uv mapping
        float u = atan2(p.y, p.x) / (2 * M_PI);
        float v = asin(p.z) / M_PI + 0.5;
        hInfo.uvw = Vec3f(u, v, 0.5);
        return true;
    }

    return false;
}

bool Plane::IntersectRay( Ray const &ray, HitInfo &hInfo, int hitSide ) const {
    float bias = 0.002;

    // if we're hitting the back and we only want the front
    if (ray.dir.z > 0 && hitSide == HIT_FRONT) return false;

    float t = -ray.p.z / ray.dir.z;

    if (t <= bias) return false;    //the plane is behind our origin point
    if (t >= hInfo.z) return false;  // we don't know if this is actually a hit or not
                                    // but if it is, it's farther away than our last, so quit

    Vec3f x = ray.p + t*ray.dir;

    if (x.x < -1 || x.x > 1 || x.y < -1 || x.y > 1) return false;   // no intersection

    // check if we're closer than previous hit before returning
    hInfo.z = t;
    hInfo.front = ray.dir.z < 0;
    hInfo.p = x;
    hInfo.N = Vec3f(0, 0, 1);
    hInfo.GN = hInfo.N;
    hInfo.uvw = (x + 1) / 2;
    return true;
}

bool TriObj::IntersectRay( Ray const &ray, HitInfo &hInfo, int hitSide ) const {
    return TraceBVHNode(ray, hInfo, hitSide, bvh.GetRootNodeID());
}

bool TriObj::IntersectTriangle( Ray const &ray, HitInfo &hInfo, int hitSide, unsigned int faceID ) const {
    float bias = 0.00002;

    TriFace face = this->F(faceID);
    Vec3f v0 = this->V(face.v[0]);
    Vec3f v1 = this->V(face.v[1]);
    Vec3f v2 = this->V(face.v[2]);

    Vec3f n_star = (v1 - v0).Cross(v2 - v0);

    float cosTheta = n_star.Dot(ray.dir);
    if (abs(cosTheta) < bias) return false; // we're basically parallel to the triangle
    if (cosTheta > bias && hitSide == HIT_FRONT) return false;  // we hit a back side and only want front

    float t = ( v0.Dot(n_star) - ray.p.Dot(n_star) ) / cosTheta;

    if ( t <= bias ) return false; // the triangle is behind the ray origin
    if ( t >= hInfo.z ) return false;   // we don't know if this is actually a hit or not
                                        // but if it is, it's farther away than our last, so quit

    Vec3f x = ray.p + t*ray.dir;    // "intersect" point

    // collapse the triangle to 2d based on the normal direction
    Vec2d v02d;
    Vec2d v12d;
    Vec2d v22d;
    Vec2d x2d;
    
    if ( abs(n_star.x) >= abs(n_star.y) && abs(n_star.x) >= abs(n_star.z) ) {
        // x is greatest component of n_star
        v02d = Vec2d(v0.y, v0.z);
        v12d = Vec2d(v1.y, v1.z);
        v22d = Vec2d(v2.y, v2.z);
        x2d = Vec2d(x.y, x.z);
    }
    else if ( abs(n_star.y) >= abs(n_star.x) && abs(n_star.y) >= abs(n_star.z) ) {
        // y is greatest component of n_star
        v02d = Vec2d(v0.x, v0.z);
        v12d = Vec2d(v1.x, v1.z);
        v22d = Vec2d(v2.x, v2.z);
        x2d = Vec2d(x.x, x.z);
    }
    else {
        // z is greatest component of n_star
        v02d = Vec2d(v0.x, v0.y);
        v12d = Vec2d(v1.x, v1.y);
        v22d = Vec2d(v2.x, v2.y);
        x2d = Vec2d(x.x, x.y);
    }

    // check if any of the areas match sign
    float area0 = (v12d - v02d).Cross(x2d - v02d);
    float area1 = (v22d - v12d).Cross(x2d - v12d);
    float area2 = (v02d - v22d).Cross(x2d - v22d);

    if ( !(((area0>=0) == (area1>=0)) && ((area1>=0) == (area2>=0))) ) return false;    //the signs don't match

    // okay, this is an actual hit, believe it or not
    // so now we can do normal interpolation
    float areaTotal = (v12d - v02d).Cross(v22d - v02d);
    float b0 = abs(area1 / areaTotal);
    float b1 = abs(area2 / areaTotal);
    float b2 = abs(area0 / areaTotal);

    Vec3f n = this->GetNormal(faceID, Vec3f(b0, b1, b2));

    hInfo.z = t;
    hInfo.front = cosTheta <= -bias;
    hInfo.p = x;
    hInfo.N = n;
    hInfo.GN = n_star.GetNormalized();
    hInfo.uvw = this->GetTexCoord(faceID, Vec3f(b0, b1, b2));
    return true;
}

bool TriObj::TraceBVHNode ( Ray const &ray, HitInfo &hInfo, int hitSide, unsigned int nodeID ) const {
    // do bounding box test on this node's bounds
    const float* bounds = bvh.GetNodeBounds(nodeID);

    float tx0 = (bounds[0] - ray.p.x) / ray.dir.x;
    float tx1 = (bounds[3] - ray.p.x) / ray.dir.x;
    float ty0 = (bounds[1] - ray.p.y) / ray.dir.y;
    float ty1 = (bounds[4] - ray.p.y) / ray.dir.y;
    float tz0 = (bounds[2] - ray.p.z) / ray.dir.z;
    float tz1 = (bounds[5] - ray.p.z) / ray.dir.z;

    if (tx0 > tx1) Swap(tx0, tx1);
    if (ty0 > ty1) Swap(ty0, ty1);
    if (tz0 > tz1) Swap(tz0, tz1);

    // we've missed this node, and return back to its parent
    if (Max(tx0, ty0, tz0) > Min(tx1, ty1, tz1)) return false;

    bool foundHit = false;

    // if this node has children, we search those now
    if (!bvh.IsLeafNode(nodeID)) {
        if ( TraceBVHNode(ray, hInfo, hitSide, bvh.GetFirstChildNode(nodeID))) foundHit = true;
        if ( TraceBVHNode(ray, hInfo, hitSide, bvh.GetSecondChildNode(nodeID))) foundHit = true;

    }
    else {
        // this means we're at a leaf node, and have to actually check the triangles here
        bvh.GetNodeElements(nodeID);
        for ( int i = 0; i < bvh.GetNodeElementCount(nodeID); i++) {
            if ( IntersectTriangle(ray, hInfo, hitSide, bvh.GetNodeElements(nodeID)[i]) ) {
                foundHit = true;
            }
        }
    }
    
    return foundHit;
}