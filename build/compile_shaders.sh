#!/bin/bash
echo "-- Vertex shaders --"
echo "Compiling [vsMVP.vert]"
glslc vsMVP.vert -o shaders/vsMVP.spv
echo "Compiling [vsVP.vert]"
glslc vsVP.vert -o shaders/vsVP.spv
echo "-- Fragment shaders --"
echo "Compiling [fsMVPSampler.frag]"
glslc fsMVPSampler.frag -o shaders/fsMVPSampler.spv
echo "Compiling [fsMVPNoSampler.frag]"
glslc fsMVPNoSampler.frag -o shaders/fsMVPNoSampler.spv

echo "Compiling [fsVPSampler.frag]"
glslc fsVPSampler.frag -o shaders/fsVPSampler.spv
echo "Compiling [fsVPNoSampler.frag]"
glslc fsVPNoSampler.frag -o shaders/fsVPNoSampler.spv

