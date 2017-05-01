#ifndef VM_SCENE_SCENE_H
#define VM_SCENE_SCENE_H
#include <array>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>

#include "scene/camera.h"
#include "scene/brush.h"

#include "gfx/texture.h"
#include "gfx/compute-context.h"

#include <boost/optional.hpp>

namespace vm {

class Scene {
    friend class Renderer;

    struct Chunk {
        boost::optional<compute::image3d> volume;
        glm::ivec3 coord;
        glm::vec3 origin;
    };

    std::shared_ptr<Camera> m_camera;
    std::shared_ptr<ComputeContext> m_compute_ctx;
    std::unordered_map<size_t, Chunk> m_chunks;
    const compute::image_format m_volume_format;
    compute::kernel m_initializer;
    compute::kernel m_downsampler;
    /* Samplers of built-in SDF functions */
    std::array<compute::kernel, 2> m_sdf_samplers;

    void persist_chunk(std::ostream &out, const Chunk &chunk) const;
    void restore_chunk(std::istream &in, Chunk &chunk);

    /**
     * Used by the Renderer to get visible chunks ordered in a front to back
     * manner.
     */
    std::vector<const Chunk *> get_chunks_to_render() const;

    /**
     * Gets the coordinates of a volume (in scene chunks) that are affected
     * by the given AABB.
     */
    void get_affected_region(glm::ivec3 &out_min,
                             glm::ivec3 &out_max,
                             const AABB &aabb) const;

    /** Returns hash of the chunk coordinates */
    static size_t get_coord_hash(int x, int y, int z);

    /** Finds node of specified coordinates or returns nullptr if nothing found */
    Chunk *get_chunk_ptr(int x, int y, int z);

    /** Returns world position of the chunk */
    glm::vec3 get_chunk_origin(int x, int y, int z) const;

    enum Operation {
        Add = 0,
        Sub = 1
    };

    compute::image3d create_volume(int N) const;

    /** Performs sampling of the brush */
    void sample(const Brush &brush, Operation op);

public:
    Scene(std::shared_ptr<ComputeContext> compute_ctx);

    /**
     * Samples @p brush over the scene adding its volume to it.
     */
    void add(const Brush &brush);

    /**
     * Samples @p brush over the scene subtracting its volume from it.
     */
    void sub(const Brush &brush);

    /**
     * Sets the current camera.
     */
    inline void set_camera(const std::shared_ptr<Camera> &camera) {
        m_camera = camera;
    }

    /** @returns the current camera. */
    inline const std::shared_ptr<Camera> get_camera() const {
        return m_camera;
    }

    /** Serializes the scene into given stream. (NOTE: this is non-portable) */
    friend std::istream &operator>>(std::istream &in, Scene &scene);

    /** Initializes the scene from the stream. (NOTE: this is non-portable) */
    friend std::ostream &operator<<(std::ostream &out, const Scene &scene);
};

} // namespace vm

#endif /* VM_SCENE_SCENE_H */
