Project compilation and usage instructions:
1) Run the following commands from the build folder -
    cmake ../CMakeLists.txt
    cmake --build .

2) The path to the scene file to be rendered must be included as a command line argument. See example below:
    ./raytracer ../scenes/scene.xml

3) To save render as PNG file: include another command line argument with a path to where you want to save the file:
    ./raytracer ../scenes/scene.xml ../images/render.png

    In this case, the OpenGL window will automatically disappear when the render has finished.

https://www.vecteezy.com/png/24819242-cow-print-black-spots-blobs-transparent-png-graphic-asset-seamless-pattern
https://freestylized.com/skybox/sky_16/
https://www.cgtrader.com/free-3d-models/space/spaceship/free-flying-saucer
