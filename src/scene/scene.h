#ifndef VM_SCENE_SCENE_H
#define VM_SCENE_SCENE_H
#include <array>
#include <fstream>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>

#include "scene/brush.h"
#include "scene/camera.h"
#include "scene/chunk.h"
#include "scene/scene-archive.h"

#include "compute/context.h"
#include "gfx/texture.h"

#include "utils/thread-pool.h"

#include "dc/mesher.h"
#include "dc/sampler.h"

#include <boost/optional.hpp>

namespace vm {

class Scene {
    friend class Renderer;

    std::shared_ptr<ComputeContext> m_compute_ctx;
    std::shared_ptr<Camera> m_camera;
    std::unordered_map<size_t, std::shared_ptr<Chunk>> m_chunks;

    SceneArchive m_archive;
    /* Dual Contouring related classes */
    dc::Sampler m_sampler;
    dc::Mesher m_mesher;
    /* Used to avoid sampling same point multiple times */
    glm::vec3 m_last_sampling_point;

    void init_persisted_chunks();

    /** Gets the region (in chunk coordinates) covered by the specified aabb */
    void get_covered_region(const AABB &region_aabb,
                            glm::ivec3 &region_min,
                            glm::ivec3 &region_max);
    /** Initializes the chunk's volumetric data */
    void init_chunk(const std::shared_ptr<Chunk> &chunk);
    /** Performs sampling of the brush */
    void sample(const Brush &brush, dc::Sampler::Operation operation);

public:
    /** Returns world position of the chunk */
    static glm::vec3 get_chunk_origin(const glm::ivec3 &coord);

    static compute::image_format samples_format();
    static compute::image_format edges_format();

    Scene(const std::shared_ptr<ComputeContext> &compute_ctx,
          const std::shared_ptr<Camera> &camera,
          const std::string &scene_directory);

    /**
     * Samples @p brush over the scene adding its volume to it.
     */
    void add(const Brush &brush);

    /**
     * Samples @p brush over the scene subtracting its volume from it.
     */
    void sub(const Brush &brush);

    /** @returns the current camera. */
    inline const std::shared_ptr<Camera> get_camera() const {
        return m_camera;
    }

    /** @returns chunks to be rendered */
    std::vector<const Chunk *> get_chunks_to_render() const;
};

} // namespace vm

#endif /* VM_SCENE_SCENE_H */
