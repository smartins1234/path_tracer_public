#include "raytracer.h"

#include <thread>
#include <iostream>
#include <limits.h>

#define BIG_INT INT_MAX-1

bool Raytracer::LoadScene( char const *sceneFilename ) {
    if (!Renderer::LoadScene(sceneFilename)) {
        return false;
    }

    // determine the camera parameters
    int width = renderImage.GetWidth();
    int height = renderImage.GetHeight();
    camH = 2 * camera.focaldist * tanf(camera.fov * M_PI / 360);
    camW = camH * float(width) / float(height);

    // camera vectors and toWorld transform
    yHat = camera.up.GetNormalized();
    zHat = -camera.dir.GetNormalized();
    xHat = yHat.Cross(zHat).GetNormalized();

    camToWorld = cy::Matrix3f(xHat, yHat, zHat);
    worldToCam = camToWorld.GetInverse();

    // set the number of pixels
    numPixels = width * height;

    return true;
}

cy::Vec3f Raytracer::CamRayDest( int i, int j, int sampleNum, float pixelOffset ) {
    // get a random sample of a unit square
    std::pair<float, float> sampleCoords = sampleGen.GetSample(sampleNum, pixelOffset);
    // turn that index into the actual camera space coordinates
    float x = -(camW / 2) + ( (camW / renderImage.GetWidth()) * (i + sampleCoords.first));
    float y = (camH / 2) - ( (camH / renderImage.GetHeight()) * (j + sampleCoords.second));

    return cy::Vec3f(x, y, -camera.focaldist);
}

Ray Raytracer::CameraRay( int i, int j, int sampleNum, float pixelOffset, float diskOffset) {
    // get ray origin offset based on camera's dof
    std::pair<float, float> diskSample = sampleGen.GetDiskSample(sampleNum, diskOffset, camera.dof);
    Vec3f diff = xHat * diskSample.first + yHat * diskSample.second;
    Vec3f rayOrg = camera.pos + diff;

    // get ray direction
    Vec3f rayDest = CamRayDest(i, j, sampleNum, pixelOffset);

    // transform to world space
    rayDest = camToWorld * rayDest + camera.pos;


    return Ray(rayOrg, rayDest - rayOrg);
}


bool Raytracer::TraceRay( Ray const &ray, HitInfo &hInfo, int hitSide ) const {
    // check if the ray intersects any objects in the scene
    bool hitObj = SearchTree(ray, hInfo, hitSide, &scene.rootNode);

    // check if the ray intersects any of the lights in the scene
    bool hitLight = false;
    for (Light* light : this->scene.lights) {
        if (!light->IsRenderable()) { continue; }

        if ( light->IntersectRay(ray, hInfo, hitSide) ) {
            hitLight = true;
            hInfo.node = nullptr;
            hInfo.isLight = true;
            hInfo.light = light;
        }
    }

    return (hitObj || hitLight);
}

bool Raytracer::ShadowTraceRay( Ray const &ray, HitInfo &hInfo, int hitSide, float t_max ) const {
    // check if the shadow ray intersects any objects in the scene
    bool hitObj = ShadowSearch(ray, hInfo, &scene.rootNode, t_max);

    // check if the shadow ray intersects any of the lights in the scene
    bool hitLight = false;
    for (Light* light : this->scene.lights) {
        if (!light->IsRenderable()) { continue; }

        if ( light->IntersectRay(ray, hInfo, hitSide) ) {
            hitLight = true;
            hInfo.node = nullptr;
            hInfo.isLight = true;
            hInfo.light = light;
        }
    }

    return (hitObj || hitLight);
}

bool Raytracer::SearchTree( Ray const &ray, HitInfo &hInfo, int hitSide, Node const *node ) const {
    // tracks if this node or any of it's descendents are the hit node
    bool descHit = false;

    // put ray in local coords
    Ray localRay =  node->ToNodeCoords(ray);

    // check for intersections
    const Object *obj = node->GetNodeObj();
    if (obj)
    {
        // check for hit
        bool isHit = node->GetNodeObj()->IntersectRay(localRay, hInfo, hitSide);
        if (isHit)
        {
            // set what node we hit
            hInfo.node = node;
            descHit = true;
        }
    }

    // continue checking the children
    for ( int i=0; i<node->GetNumChild(); i++ ) 
    {
        // using localRay bc transformations stack
        if ( SearchTree(localRay, hInfo, hitSide, node->GetChild(i)) )
        {
            descHit = true;
        }
    }

    if (descHit)
    {
        // put our hit info into this node's coordinate system
        node->FromNodeCoords(hInfo);
    }
    return descHit;
}

bool Raytracer::ShadowSearch( Ray const &ray, HitInfo &hInfo, Node const *node, float t_max ) const {
    // put ray in local coords
    Ray localRay =  node->ToNodeCoords(ray);

    // check for intersections
    const Object *obj = node->GetNodeObj();
    if (obj)
    {
        // check for hit
        bool isHit = node->GetNodeObj()->IntersectRay(localRay, hInfo, HIT_FRONT_AND_BACK);
        if (isHit && hInfo.z < t_max)
        {
            // we're done!
            return true;
        }
    }

    // continue checking the children
    for ( int i=0; i<node->GetNumChild(); i++ ) 
    {
        // using localRay bc transformations stack
        if ( ShadowSearch(localRay, hInfo, node->GetChild(i), t_max) )
        {
            return true;
        }
    }

    return false;
}

Color Raytracer::tracePath(Ray ray, SamplerInfo sInfo, HitInfo& hInfo, int bounce) {
    if ( bounce >= 2000 ) {
        // return when we've reached the maximum number of bounces
        return Color().Black();
    }

    // trace the given ray through the scene
    hInfo.Init();
    bool hit = TraceRay(ray, hInfo, HIT_FRONT_AND_BACK);

    sInfo.SetHit(ray, hInfo);

    // if we've hit nothing, the hit distance is effectively infinity
    if ( !hit ) {
        hInfo.z = BIGFLOAT;
    }

    // sampling time t
    float roll = sInfo.RandomFloat();
    float tRand = -log(1-roll) / sig_t;

    // if our time sample is less than the hit "time" (aka distance)
    if ( tRand < hInfo.z ) {
        if ( roll < (sig_a / sig_t) ) { // russian roulette absorption/emmision
            // treating "absorption" as emision of the background color
            if ( bounce == 0 && !hit ) {
                Vec3f uvw(float(sInfo.X())/renderImage.GetWidth(), float(sInfo.Y())/renderImage.GetHeight(), 0.5);
                return scene.background.Eval(uvw);
            }
            else if ( !hit ) {
                return scene.environment.EvalEnvironment(ray.dir);
            }
            return Color().Black();
        }

        // probability and transmittance of this time sample
        float pdf = exp(-sig_t * tRand) * sig_t;
        float transmittance = exp(-sig_t * tRand);

        // calculate the point we're scattering from
        Vec3f p = ray.p + tRand * ray.dir;

        Color lightSampColor = Color().Black();

        // sample the lights to get a new direction
        HitInfo shadowInfo(hInfo);
        shadowInfo.p = p;
        SamplerInfo lSampInfo(sInfo);
        lSampInfo.SetHit(ray, shadowInfo);
        Light* light = this->randomLight(sInfo);
        Vec3f lDir;
        DirSampler::Info lInfo;
        lInfo.SetVoid();

        bool sample = light->GenerateSample(lSampInfo, lDir, lInfo);
        if ( sample ) { // if we get a non-zero sample
            // adjust the samples probability
            lInfo.prob /= lightsRenderable.size();

            // check if this sample is in shadow
            shadowInfo.Init();
            bool shadowHit = ShadowTraceRay(Ray(p, lDir), shadowInfo, HIT_FRONT_AND_BACK, 1.0);

            // get color value from the light sample
            if ( (shadowHit && shadowInfo.isLight && shadowInfo.light == light) ) {
                // there's nothing between the light we sampled and the point
                float l_transmit = exp(-sig_t * shadowInfo.z * lDir.Length());
                float l_pdf = exp(-sig_t * shadowInfo.z * lDir.Length());
                
                // multiple importance sampling weight calculation
                lightSampColor = l_transmit / l_pdf * lInfo.mult;
                float lightToPhase = 1 / (4 * M_PI) * l_pdf;
                lightSampColor *= lightToPhase;

                float w = (lInfo.prob * lInfo.prob) / ( (lInfo.prob * lInfo.prob) + (lightToPhase * lightToPhase) );
                lightSampColor *= w;
            }
        }

        // sample the phase function to get a new direction
        float cosTheta = (2 * sInfo.RandomFloat()) - 1;
        float sinTheta = sqrt(1 - pow(cosTheta, 2));
        float phi = 2 * M_PI * sInfo.RandomFloat();
        Vec3f dirNew(sinTheta*cos(phi), sinTheta*sin(phi), cosTheta);

        // and recurse
        Color samp2 = tracePath(Ray(p, dirNew), sInfo, hInfo, bounce+1);
        float w2 = 0.5;

        Color total = ( samp2 * w2 ) + lightSampColor;

        return transmittance / pdf * sig_s * total;
    }
    else if ( hit ) {   // if we don't scatter before hitting a surface
        // get the probability and transmittance of this hit
        float pdf = exp(-sig_t * hInfo.z);
        float transmittance = exp(-sig_t * hInfo.z);

        // if we hit a light
        if ( hInfo.isLight ) {
            if ( bounce == 0 ) { hInfo.light->Radiance(sInfo); }
            return Color().Black();
        }
        else {
            // if we hit a surface, we need to sample it's brdf and the lights as in typical path tracing
            return transmittance / pdf * materialSample(ray, sInfo, hInfo, bounce);
        }
    }

    if ( bounce == 0 ) {
        Vec3f uvw(float(sInfo.X())/renderImage.GetWidth(), float(sInfo.Y())/renderImage.GetHeight(), 0.5);
        return scene.background.Eval(uvw);
    }
    return scene.environment.EvalEnvironment(ray.dir);
}

Color Raytracer::materialSample( Ray ray, SamplerInfo sInfo, HitInfo& hInfo, int bounce ) {
    // get a random light to sample
    Light* light = this->randomLight(sInfo);
    int startBounce = bounce;

    // sample the material's brdf
    Vec3f mDir;
    DirSampler::Info mInfo;
    mInfo.SetVoid();
    bool sample = hInfo.node->GetMaterial()->GenerateSample(sInfo, mDir, mInfo);
    if ( !sample ) {
        return mInfo.mult;
    }
    if ( mInfo.lobe == DirSampler::SPECULAR ) {
        if ( mDir.Dot(sInfo.GN()) < 0 ) {
            mInfo.mult = Color().Black();
        }
    }

    // setup for MIS
    Color matColor = mInfo.mult / mInfo.prob;
    DirSampler::Info matToL;
    matToL.SetVoid();
    light->GetSampleInfo(sInfo, mDir, matToL);

    HitInfo giInfo;
    giInfo.Init();
    Color gi = Color().Black();
    if (matToL.prob == 0 && !mDir.IsZero()) {
        // continue tracing paths until we hit a light, run out of bounces, or the light is russian-roulette "absorbed"
        gi = tracePath(Ray(sInfo.P(), mDir), sInfo, giInfo, bounce+1);
        matColor *= gi;
    }
    bounce = startBounce;

    // sample the random light
    Vec3f lDir;
    DirSampler::Info lInfo;
    lInfo.SetVoid();
    bool lightSample = light->GenerateSample(sInfo, lDir, lInfo);
    lInfo.prob /= lightsRenderable.size();

    Color lightColor = Color().Black();
    if ( lightSample && lInfo.prob > 0 ) {  // if get a non-zero light sample
        HitInfo shadowInfo;
        shadowInfo.Init();

        // check if our sample is actually in shadow
        bool shadowHit = ShadowTraceRay(Ray(sInfo.P(), lDir), shadowInfo, HIT_FRONT_AND_BACK, 1.0f);
        bool hitSelf = (shadowHit && shadowInfo.isLight && shadowInfo.light == light);
        if ( shadowHit && !hitSelf ) {
            lInfo.mult = Color().Black();
        }

        lDir.Normalize();

        // setup for MIS
        lightColor = lInfo.mult / lInfo.prob;
        DirSampler::Info lToMat;
        lToMat.SetVoid();
        hInfo.node->GetMaterial()->GetSampleInfo(sInfo, lDir, lToMat);

        if ( lToMat.prob > 0 ) {
            lightColor *= lToMat.mult;

            float l1 = lInfo.prob * lInfo.prob;
            float l2 = lToMat.prob * lToMat.prob;
            float wLight = l1 / (l1 + l2);

            lightColor *= wLight;
        }
        else {
            lightColor = Color().Black();
        }
    }
    
    float m1 = mInfo.prob * mInfo.prob;
    float m2 = matToL.prob * matToL.prob;

    float wMat =   m1 / (m1 + m2);

    // MIS combination of our light and material samples
    Color total = lightColor + matColor * wMat;
    return total;
}

Color Raytracer::samplePixel( float pixelOffset, float dofOffset, int sampleNum, HitInfo& hInfo, SamplerInfo& sInfo, float& z ){
    // generate a ray
    Ray ray = CameraRay(sInfo.X(), sInfo.Y(), sampleNum, pixelOffset, dofOffset);
    Color total = Color().Black();

    // trace a path starting with that ray
    total = tracePath(ray, sInfo, hInfo);
    z = hInfo.z;
    return total;
}

Light* Raytracer::randomLight(SamplerInfo const &sInfo) {
    int myRand = sInfo.RandomInt() % lightsRenderable.size();
    return lightsRenderable[myRand];
}

void Raytracer::RenderPixels() {
    int index = next++;
    HitInfo info;
    RNG rng(index);
    SamplerInfo sInfo(rng);

    int width = renderImage.GetWidth();
    int height = renderImage.GetHeight();

    while (index < numPixels) {
        int i = index % width;
        int j = index / width;
        sInfo.SetPixel(i, j);

        // antialiasing offset for this pixel
        float pixOffset = rng.RandomFloat();

        // dof offset for this pixel
        float dofOffset = rng.RandomFloat();

        Color S1 = Color().Black();
        Color S2 = Color().Black();
        float z_min = BIGFLOAT;
        int sampNum;

        // sample the pixel the given number of times
        for (sampNum = 0; sampNum < sampleMax; ++sampNum) {
            sInfo.SetPixelSample(sampNum);
            float z = 0;
            Color sample = samplePixel( pixOffset, dofOffset, sampNum, info, sInfo, z );
            if (z < z_min) z_min = z;

            S1 += sample;
        }

        Color color = S1 / float(sampNum + 1);
        if (camera.sRGB) {
            color = color.Linear2sRGB();
        }

        renderImage.GetPixels()[index] = Color24(color);
        renderImage.GetZBuffer()[index] = z_min;
        renderImage.GetSampleCount()[index] = sampNum;

        // update number of rendered pixels
        renderImage.IncrementNumRenderPixel(1);

        if (renderImage.IsRenderDone()) {
            // only one thread should ever get here, and it should be the last one
            isRendering = false;
            return;
        }

        index = next++;
    }
}

void Raytracer::BeginRender() {
    for ( auto l : this->scene.lights ) {
        if (l->IsPhotonSource()) {
            lightsRenderable.push_back(l);
        }
    }

    std::vector<std::thread> threads;

    int n = std::thread::hardware_concurrency() / 2;
    if (n == 0) n = 8;
#ifndef NDEBUG
    n = 1;
#endif
    fprintf(stdout, "Rendering with %d threads\n", n);

    isRendering = true;

    for( int i = 0; i < n; i++ )
    {
        threads.emplace_back(std::thread(&Raytracer::RenderPixels, this));
    }
    for( auto& i : threads )
    {
        i.detach();
    }
}

void Raytracer::StopRender () {

}
