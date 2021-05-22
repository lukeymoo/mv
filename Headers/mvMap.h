#pragma once

#include <vector>

class MapHandler
{
  public:
    MapHandler();
    ~MapHandler();

    enum NoiseType
    {
        ePerlin = 0,
        eOpenSimplex,
    };

    struct Coordinate
    {
        float x;
        float y;
        float z;
    };

    std::vector<Coordinate> generateNoise(NoiseType noiseType);
};
