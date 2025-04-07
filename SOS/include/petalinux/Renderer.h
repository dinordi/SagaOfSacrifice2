#pragma once
#include <vector>
#include "object.h"

class Renderer
{
public:
    Renderer() = default;
    ~Renderer() = default;

    void init();
    void render(std::vector<Object*> objects);
};