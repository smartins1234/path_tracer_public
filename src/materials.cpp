#include "materials.h"
#include "raytracer.h"
#include "lights.h"

#include <iostream>

bool MtlPhong::GenerateSample( SamplerInfo const &sInfo, Vec3f &dir, Info &si ) const {}
void MtlPhong::GetSampleInfo ( SamplerInfo const &sInfo, Vec3f const &dir, Info &si ) const {};

bool MtlBlinn::GenerateSample( SamplerInfo const &sInfo, Vec3f &dir, Info &si ) const {
    float dPow = Diffuse().Eval(sInfo.UVW()).Max();
    float rPow = Specular().Eval(sInfo.UVW()).Max();
    float tPow = Refraction().Eval(sInfo.UVW()).Max();

    float sum = dPow + rPow + tPow;
    if (sum >= 1) {
        dPow /= 2.0f * sum;
        rPow /= 2.0f * sum;
        tPow /= 2.0f * sum;
    }

    float roll = sInfo.RandomFloat();
    if ( roll < dPow ) {
        // set this photon's lobe
        si.lobe = Lobe::DIFFUSE;
        
        // cosine weighted random direction
        Vec3f u, v;
        sInfo.N().GetOrthonormals(u, v);

        float phi = sInfo.RandomFloat() * 2 * M_PI;
        float cosTheta = sqrt(1 - sInfo.RandomFloat());
        float sinTheta = sqrt(1 - pow(cosTheta, 2));
        
        dir = sInfo.N()*cosTheta + u*sinTheta*cos(phi) + v*sinTheta*sin(phi);

        // set this photon's probablity
        si.prob = dPow * cosTheta / M_PI;
        si.mult = cosTheta * Diffuse().Eval(sInfo.UVW()) / M_PI;
        return true;
    }

    // reflection/transmission setup
    Vec3f norm;
    float eta;
    if ( sInfo.IsFront() ){   //frontface hit
        eta = 1.0f / ior;
        norm = sInfo.N();
    }
    else {  //backface hit
        eta = ior;
        norm = -sInfo.N();
    }

    // glossy normal sample
    norm.Normalize();
    Vec3f u, v;
    norm.GetOrthonormals(u, v);
    float gloss = Glossiness().Eval(sInfo.UVW());
    float cosTheta = pow(1 - sInfo.RandomFloat(), 1.0f / (gloss + 1.0f));
    float sinTheta = sqrt(1 - (cosTheta * cosTheta));
    float phi = sInfo.RandomFloat() * 2 * M_PI;

    Vec3f half = norm * cosTheta + u * sinTheta * cos(phi) + v * sinTheta * sin(phi);
    
    if ( roll < dPow + rPow ) {
        si.lobe = Lobe::SPECULAR;

        // calculate the reflection direction
        Vec3f rDir = -sInfo.V() + ( 2 * half.Dot(sInfo.V()) * half);
        dir = rDir;

        // set this photon's probablity
        si.prob = rPow * (gloss + 1) / (2 * M_PI) * pow(cosTheta, gloss+1) / (4);

        float cosOut = rDir.Dot(norm);
        if (cosOut < 0) {
            dir = Vec3f(0.0f);
            return false;
        }

        // set this photon's bsdf*geometry term
        float specCons = (Glossiness().Eval(sInfo.UVW()) + 2) / (8 * M_PI);
        Color f_spec = pow(norm.Dot(half), gloss) * Specular().Eval(sInfo.UVW()) * specCons / cosOut;
        si.mult = cosOut * f_spec;// / si.prob;

        return true;
    }
    else if ( roll < dPow + rPow + tPow ) {
        si.lobe = Lobe::TRANSMISSION;

        float k_cosTheta = sInfo.V().Dot(half);
        float cosPhi_2 = 1 - (pow(eta, 2) * (1 - pow(k_cosTheta, 2)));

        // set this photon's probablity
        si.prob = tPow * (gloss + 1) / (2 * M_PI) * pow(cosTheta, gloss+1) / 4;

        if (half.Dot(sInfo.V()) < 0){
            dir = Vec3f(0.0f);
            return false;
        }
        // if cosPhi_2 is negative, reflect
        if ( cosPhi_2 < 0 ) {
            dir = Vec3f(0.0f);
            return false;
        }

        // get the transmission direction
        dir = (-eta * sInfo.V()) - (sqrt(cosPhi_2) - (eta * k_cosTheta)) * half;
        float cosOut = abs(norm.Dot(dir));

        // set this photon's bsdf*geometry term
        float specCons = (Glossiness().Eval(sInfo.UVW()) + 2) / (8 * M_PI);
        Color f_trans = pow(norm.Dot(half), gloss) * Refraction().Eval(sInfo.UVW()) * specCons / cosOut;
        si.mult = cosOut * f_trans;
        return true;
    }
    si.prob = 1.0f - (dPow + rPow + tPow);

    Color emit = Emission().Eval(sInfo.UVW());
    if (isnan(emit.r)) {
            emit = Color().Black();
    }
    if (!emit.IsBlack()) {si.mult = emit; dir = sInfo.N(); return false;}

    dir = Vec3f(0.0f);
    si.mult = Color().Black();
    return false;
}
void MtlBlinn::GetSampleInfo ( SamplerInfo const &sInfo, Vec3f const &dir, Info &si ) const {
    float dPow = Diffuse().Eval(sInfo.UVW()).Max();
    float rPow = Specular().Eval(sInfo.UVW()).Max();
    float tPow = Refraction().Eval(sInfo.UVW()).Max();

    float sum = dPow + rPow + tPow;
    if (sum >= 1) {
        dPow /= 2.0f * sum;
        rPow /= 2.0f * sum;
        tPow /= 2.0f * sum;
    }

    si.prob = 0.0f;
    si.mult = Color().Black();

    
    Vec3f norm = sInfo.N();

    if ( sInfo.V().Dot(norm) > 0 == dir.Dot(norm) > 0 ) {
        float cosOut = sInfo.N().Dot(dir);

        // diffuse
        if (cosOut > 0) {
            si.mult += cosOut * Diffuse().Eval(sInfo.UVW()) / M_PI;
            si.prob += dPow / M_PI;
        }

        // specular
        if (cosOut < 0) norm *= -1;
        float gloss = Glossiness().Eval(sInfo.UVW());
        float specCons = (gloss + 2) / (8 * M_PI);

        Vec3f half = (sInfo.V() + dir).GetNormalized();
        float geoTerm = norm.Dot(half);

        si.mult += pow(norm.Dot(half), gloss) * Specular().Eval(sInfo.UVW()) * specCons;
        si.prob += (gloss + 1) * pow(geoTerm, gloss) * rPow;
    }

    else {
        float eta;
        if ( dir.Dot(norm) >= 0 ){   //frontface hit
            eta = 1.0f / ior;
        }
        else {  //backface hit
            eta = ior;
            norm *= -1;
        }

        float gloss = Glossiness().Eval(sInfo.UVW());
        float specCons = (gloss + 2) / (8 * M_PI);

        Vec3f half = (dir + eta * sInfo.V()).GetNormalized();
        float geoTerm = half.Dot(norm);

        si.mult += pow(geoTerm, gloss) * Refraction().Eval(sInfo.UVW()) * specCons;
        si.prob += (gloss + 1) * pow(geoTerm, gloss) * tPow;
    }

    Color emit = Emission().Eval(sInfo.UVW());
    si.mult += emit;
    if (!emit.IsBlack()) si.prob += 1 - (dPow + rPow + tPow);
};

bool MtlMicrofacet::GenerateSample( SamplerInfo const &sInfo, Vec3f &dir, Info &si ) const {}
void MtlMicrofacet::GetSampleInfo ( SamplerInfo const &sInfo, Vec3f const &dir, Info &si ) const {};

