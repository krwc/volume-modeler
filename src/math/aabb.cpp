#include "aabb.h"

#include <cassert>
#include <numeric>

namespace vm {
namespace math {
using namespace glm;

namespace {
const float MAX = std::numeric_limits<float>::max();
const float MIN = -MAX;
} // namespace

AABB::AABB() : AABB({ MAX, MAX, MAX }, { MIN, MIN, MIN }) {}

AABB::AABB(const vec3 &min, const vec3 &max) : min(min), max(max) {}

void AABB::cover(const vec3 &point) {
    min = glm::min(min, point);
    max = glm::max(max, point);
}

namespace {
vec3 get_aabb_vertex(const AABB &aabb, size_t index) {
    switch (index) {
    case 0: return aabb.min;
    case 1: return aabb.max;
    case 2: return vec3(aabb.min.x, aabb.min.y, aabb.max.z);
    case 3: return vec3(aabb.min.x, aabb.max.y, aabb.min.z);
    case 4: return vec3(aabb.min.x, aabb.max.y, aabb.max.z);
    case 5: return vec3(aabb.max.x, aabb.min.y, aabb.min.z);
    case 6: return vec3(aabb.max.x, aabb.min.y, aabb.max.z);
    case 7: return vec3(aabb.max.x, aabb.max.y, aabb.min.z);
    default: assert(0 && "invalid index"); return vec3(NAN, NAN, NAN);
    }
}
} // namespace

AABB AABB::transform(const mat3 &transformation) const {
    AABB transformed_aabb;
    for (size_t index = 0; index < 8; ++index) {
        transformed_aabb.cover(transformation * get_aabb_vertex(*this, index));
    }
    return transformed_aabb;
}

} // namespace math
} // namespace vm
