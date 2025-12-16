#include <iostream>

#include "raytracer.h"

Raytracer tracer(256, 256);
SampleGenerator sampleGen = SampleGenerator::GetGenerator(256);

int main(int argc, char** argv)
{
    // get scene file path
    if (argc < 2 || argc > 3)
    {
        fprintf(stderr, "Must provide a scene file. See options below:\n"
        "\t./main path/to/<sceneFile>.xml\n"
        "\t./main path/to/<sceneFile>.xml path/to/rendered/<image>.png\n"
        );
        return EXIT_FAILURE;
    }
    char const *scene_path = argv[1];

    
    tracer.LoadScene(scene_path);

    // if saving a png, save image when done and close
    if (argc == 3) {
        ShowViewport(&tracer, true);

        while (tracer.IsRendering()) {}

        char const *image_path = argv[2];
        bool saved = tracer.GetRenderImage().SaveImage(image_path);
        if (!saved) {
            fprintf(stderr, "Could not save PNG file.\n");
            return EXIT_FAILURE;
        }
    }
    else {
        ShowViewport(&tracer);
    }
    

    return EXIT_SUCCESS;
}