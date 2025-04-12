#ifndef GEOMETRY_HH
#define GEOMETRY_HH

#include <tuple>
#include <vector>

typedef std::tuple<int, int> point;

struct sector
{
    std::vector<point> verts;
    std::vector<sector *> links;
};

#endif