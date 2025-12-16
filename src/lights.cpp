#include "lights.h"
#include "raytracer.h"
#include <iostream>

// check if the given ray intersects this light
bool PointLight::IntersectRay( Ray const &ray, HitInfo &hInfo, int hitSide ) const {

    float bias = 0.002;
    bool front = true;

    // q is pos and r = size
    Vec3f diff = ray.p - position;
    float a = ray.dir.Dot(ray.dir);
    float b = 2 * ray.dir.Dot(diff);
    float c = diff.Dot(diff) - (size * size);

    float delta = (b * b) - (4 * a * c);
    if (delta < 0) return false;

    float t = ( -b - sqrt(delta) ) / ( 2 * a );
    if ( t <= 0 ) { return false; }
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

bool PointLight::GenerateSample( SamplerInfo const &sInfo, Vec3f       &dir, Info &si ) const { 
    // generate a random point on the "disk" that is our light
    Vec3f diskNorm = position-sInfo.P();
    float radius = sqrt(diskNorm.LengthSquared() - size * size) * size / diskNorm.Length();
    float sampleRadius = sqrt(sInfo.RandomFloat()) * this->size;
    float theta = sInfo.RandomFloat() * 2 * M_PI;
    float x_offset = sampleRadius * cos(theta);
    float y_offset = sampleRadius * sin(theta);

    Vec3f u, v;
    (this->position-sInfo.P()).GetNormalized().GetOrthonormals(u, v);
    Vec3f sampPoint = this->position + u*x_offset + v*y_offset;

    // get the direction towards this sample
    dir = sampPoint - sInfo.P();

    // calculate the probability and energy of this sample
    si.prob = 1 / ( radius * radius * M_PI );
    si.mult = this->intensity / dir.LengthSquared();

    return true;
};

// get the sample information of this light given a brdf sample direction
void PointLight::GetSampleInfo ( SamplerInfo const &sInfo, Vec3f const &dir, Info &si ) const {
    // check if this sample could hit our light
    HitInfo hInfo;
    hInfo.Init();
    bool hit = this->IntersectRay(Ray(sInfo.P(), dir), hInfo, HIT_FRONT);
    if (hit) {
        // if so, return the probability of this light generating that sample
        // as well as what energy said sample would have
        Vec3f diskNorm = position-sInfo.P();
        float radius = sqrt(diskNorm.LengthSquared() - size * size) * size / diskNorm.Length();

        Vec3f diff = hInfo.p - sInfo.P();

        si.prob = 2 * radius * radius / diff.LengthSquared();
        si.mult = this->intensity / diff.LengthSquared() * 4 * M_PI * radius * radius;
    }
    else {
        si.prob = 0;
        si.mult = Color().Black();
    }
};

//----------------------------------------------------------------

bool SpotLight::IntersectRay( Ray const &ray, HitInfo &hInfo, int hitSide ) const {
    Vec3f rayDir = -ray.dir.GetNormalized();
    Vec3f normDir = this->direction.GetNormalized();
    if (rayDir.Dot(normDir) < 0) { return false; }

    bool hitSphere = PointLight::IntersectRay(ray, hInfo, hitSide);
    if (!hitSphere) return false;

    Vec3f hitDir = (hInfo.p - this->position).GetNormalized();
    float hitRadius = sqrt(1 - pow(hitDir.Dot(normDir), 2));

    float radius = sin(this->angle);
    if (hitRadius > radius) {
        return false;
    }

    return true;
}

bool SpotLight::GenerateSample( SamplerInfo const &sInfo, Vec3f       &dir, Info &si ) const { 
    Vec3f pDir = (sInfo.P() - this->position).GetNormalized();
    if ( pDir.Dot(this->direction.GetNormalized()) < cos(this->angle) ) {
        // this point lies without the cone
        si.mult = Color().Black();
        si.prob = 0;
        dir = Vec3f();
        return false;
    }

    // generate a random point on the "disk" that is our light
    float radius = sin(angle) * size;

    float sampleRadius = sqrt(sInfo.RandomFloat()) * radius;
    float theta = sInfo.RandomFloat() * 2 * M_PI;
    float x_offset = sampleRadius * cos(theta);
    float y_offset = sampleRadius * sin(theta);

    Vec3f u, v;
    (this->position-sInfo.P()).GetNormalized().GetOrthonormals(u, v);
    Vec3f sampPoint = this->position + u*x_offset + v*y_offset;

    dir = sampPoint - sInfo.P();

    si.prob = 1 / ( radius * radius * M_PI );
    si.mult = this->intensity / dir.LengthSquared();

    return true;
};
void SpotLight::GetSampleInfo ( SamplerInfo const &sInfo, Vec3f const &dir, Info &si ) const {
    Vec3f pDir = (sInfo.P() - this->position).GetNormalized();
    float cosPos = pDir.Dot(this->direction.GetNormalized());
    if ( cosPos < cos(this->angle) ) {
        // this point lies without the cone
        si.mult = Color().Black();
        si.prob = 0;
        return;
    }

    HitInfo hInfo;
    hInfo.Init();
    bool hit = this->IntersectRay(Ray(sInfo.P(), dir), hInfo, HIT_FRONT);
    if (hit) {
        float radius = sin(this->angle);
        Vec3f diff = hInfo.p - sInfo.P();

        si.prob = 2 * radius * radius / diff.LengthSquared();
        si.mult = this->intensity / diff.LengthSquared() * 4 * M_PI * radius * radius;
    }
    else {
        si.prob = 0;
        si.mult = Color().Black();
    }
};
