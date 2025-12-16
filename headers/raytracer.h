#ifndef _RAYTRACER_H_INCLUDED_
#define _RAYTRACER_H_INCLUDED_

#include "renderer.h"
#include "rng.h"
#include "photonmap.h"

// a class for using Halton sequences to produce pseudo-random samples
// of pixels and disks
class SampleGenerator
{
private:
    int                sCount;  // number of samples
    std::vector<float> xVals;   // x values for pixel sampling
    std::vector<float> yVals;   // y values for pixel sampling
    std::vector<float> radii;   // radius values for disk sampling
    std::vector<float> angles;  // angle values for disk sampling

    SampleGenerator(int sampleCount) : sCount(sampleCount) {
        xVals.resize(sCount);
        yVals.resize(sCount);
        radii.resize(sCount);
        angles.resize(sCount);

        // initialize each vector with their Halton values
        for (int i = 0; i < sCount; i++) {
            xVals[i] = Halton(i, 2);
            yVals[i] = Halton(i, 3);
            radii[i] = Halton(i, 5);
            angles[i] = Halton(i, 7);
        }
    }

public:
    static SampleGenerator& GetGenerator(int sampleCount) {
        static SampleGenerator instance(sampleCount);
        return instance;
    }

    // get a sample within a pixel, with an additional random offset
    std::pair<float, float> GetSample(int sampleNum, float offset) {
        float xCoord = xVals[sampleNum] + offset;
        float yCoord = yVals[sampleNum] + offset;
        if (xCoord > 1.0f) xCoord -= 1.0f;
        if (yCoord > 1.0f) yCoord -= 1.0f;
        return std::pair<float, float>(xCoord, yCoord);
    }

    // get a sample within a disk, with an additional random offset and with a given radius
    std::pair<float, float> GetDiskSample(int sampleNum, float offset, float r) {
        float radius = sqrt(radii[sampleNum]) + offset;
        float theta = angles[sampleNum] + offset;
        // if (radius > 1.0f) radius -= 1.0f;
        if (radius > 1.0f) radius = 2.0f - radius;
        if (theta > 1.0f) theta -= 1.0f;
        radius *= r;
        theta *= 2.0f * M_PI;
        return std::pair<float, float>(radius * cos(theta), radius * sin(theta));
    }
};

extern SampleGenerator sampleGen;

class Raytracer : public Renderer
{
private:
    int numPixels;          // total number of pixels in the output image
    int bounceMax;          // maximum number of bounces
    int sampleMin;          // minimum number of pixel samples
    int sampleMax;          // maximum number of pixel samples
    std::atomic<int> next;  // index of the next pixel to be rendered

    float camW;                 // width of the camera pane in world space
    float camH;                 // height of the campera pane in world space
    float t_95_2 = 2.15 * 2.15; // estimated t-value^2 for adaptive sampling at 95% confidence
    float t_96_2 = 2.27 * 2.27; // estimated t-value^2 for adaptive sampling at 96% confidence
    float t_98_2 = 2.63 * 2.63; // estimated t-value^2 for adaptive sampling at 98% confidence
    float t_99_2 = 3.00 * 3.00; // estimated t-value^2 for adaptive sampling at 99% confidence

    Vec3f xHat, yHat, zHat;     // camera orientation vectors

    cy::Matrix3f camToWorld;    // camera to world space matrix
    cy::Matrix3f worldToCam;    // world to camera space matrix

    PhotonMap* pMap = nullptr;              // photon map
    std::vector<Light*> lightsRenderable;   // list of renderable lights

    // global volume parameters
    float sig_a = 0.15f;
    float sig_s = 0.06f;
    float sig_t = sig_a + sig_s;

public:
    Raytracer(int minSamples, int maxSamples)
        : sampleMax(maxSamples), sampleMin(minSamples)
    { next = 0; pMap = new PhotonMap(); }

    ~Raytracer() { if (pMap != nullptr) { delete pMap; }}

    int GetMaxBounce() const { return bounceMax; }

    bool LoadScene( char const *sceneFilename ) override;

    void BeginRender() override;
	void StopRender () override;

    // trace a ray through the scene
	bool TraceRay( Ray const &ray, HitInfo &hInfo, int hitSide=HIT_FRONT_AND_BACK ) const override;
    // trace a shadow ray through the scene
    bool ShadowTraceRay( Ray const &ray, HitInfo &hInfo, int hitSide, float t_max ) const;

    // search the scene tree for an intersection
    bool SearchTree( Ray const &ray, HitInfo &hInfo, int hitSide, Node const *node ) const;
    // search the scene tree specifically for a shadow ray intersection
    bool ShadowSearch ( Ray const &ray, HitInfo &hInfo, Node const *node, float t_max ) const;

    // construct a photon map
    void BuildPhotonMap() const;

private:
    // get a random camera ray destination for a specific pixel and sample number
    cy::Vec3f CamRayDest( int i, int j, int sampleNum, float pixelOffset );
    // get a random camera ray for a specific pixel and sample number
    Ray CameraRay( int i, int j, int sampleNum, float pixelOffset, float diskOffset);
    // worker thread render loop
    void RenderPixels();
    // a single sample of a specific pixel
    Color samplePixel( float pixelOffset, float dofOffset, int sampleNum, HitInfo& info, SamplerInfo& sInfo, float& z );
    // trace a path through the scene
    Color tracePath( Ray ray, SamplerInfo sInfo, HitInfo& hInfo, int bounce=0 );
    // light energy output based on a material surface as opposed to a volume
    Color materialSample( Ray ray, SamplerInfo sInfo, HitInfo& hInfo, int bounce=0 );
    // select a random light in the scene
    Light* randomLight(SamplerInfo const &sInfo);
};

extern Raytracer tracer;

#endif