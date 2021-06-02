#!/bin/bash
echo "-- Vertex shaders --"
echo "Compiling [vsMVP.vert]"
glslc vsMVP.vert -o ../build/shaders/vsMVP.spv
echo "Compiling [vsVP.vert]"
glslc vsVP.vert -o ../build/shaders/vsVP.spv
echo "-- Fragment shaders --"
echo "Compiling [fsMVPSampler.frag]"
glslc fsMVPSampler.frag -o ../build/shaders/fsMVPSampler.spv
echo "Compiling [fsMVPNoSampler.frag]"
glslc fsMVPNoSampler.frag -o ../build/shaders/fsMVPNoSampler.spv

echo "Compiling [fsVPSampler.frag]"
glslc fsVPSampler.frag -o ../build/shaders/fsVPSampler.spv
echo "Compiling [fsVPNoSampler.frag]"
glslc fsVPNoSampler.frag -o ../build/shaders/fsVPNoSampler.spv

