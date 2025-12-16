# Path Tracer

![alt text](https://github.com/smartins1234/path_tracer_public/blob/main/images/render06_4096_cc.png?raw=true)

Rendered at 1200 x 1400 resolution with 4096 samples per pixel. Render time: 0:15:33

## Implemented Features

- Energy-conserving Blinn material model

- Antialiasing using Quasi-Monte Carlo pixel sampling

- Depth of field using Quasi-Monte Carlo disk sampling

- Soft shadows

- Glossy reflections and refractions

- Next Event Estimation and Multiple Importance Sampling

- Russian Roulette path termination scheme

- Volumetric path tracing using MIS to sample both the phase function and the light sources

## A Note on Copyrighted Files

This path tracer was created as part of the course CS 6620: Rendering with Ray Tracing taught by Dr. Cem Yuksel at the University of Utah. Some copyrighted code was provided to students, which I used in creating this implementation but do not have the rights to share publicly. These mostly consisted of header files that encouraged a certain project structure but little to no implementation. All of the features of this path tracer listed above were fully written myself.

## Sources

UFO model and textures: https://www.cgtrader.com/free-3d-models/space/spaceship/free-flying-saucer

Utah Teapot model: Cem Yuksel, University of Utah. https://graphics.cs.utah.edu/teapot/

Cow print texture: https://www.vecteezy.com/png/24819242-cow-print-black-spots-blobs-transparent-png-graphic-asset-seamless-pattern

Background image: NASA https://svs.gsfc.nasa.gov/4851
