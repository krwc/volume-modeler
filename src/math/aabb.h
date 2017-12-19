#ifndef VM_MATH_AABB_H
#define VM_MATH_AABB_H

#include <glm/glm.hpp>

namespace vm {
namespace math {

struct AABB {
    glm::vec3 min;
    glm::vec3 max;

    /**
     * Initializes an empty, invalid AABB. It can be later made valid
     * by calling one of the AABB#cover overloads.
     */
    AABB();

    /**
     * Initializes an AABB out of provided @ref min and @ref max coordinates.
     */
    AABB(const glm::vec3 &min, const glm::vec3 &max);

    /**
     * Ensures that the given @ref point is enclosed within this AABB.
     */
    void cover(const glm::vec3 &point);

    /**
     * Transforms this AABB (i.e. the box) by the provided matrix and
     * returns a new AABB such that it encloses the transformed AABB entirely.
     *
     * @param transformation    Matrix representing linear transformation to be
     *                          applied.
     */
    AABB transform(const glm::mat3 &transformation) const;
};

} // namespace math
} // namespace vm

#endif /* VM_MATH_AABB_H */
