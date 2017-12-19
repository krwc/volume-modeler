#ifndef VM_SCENE_SCENE_ARCHIVE_H
#define VM_SCENE_SCENE_ARCHIVE_H
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <tuple>

#include "chunk.h"

#include <compute/context.h>
#include <utils/thread-pool.h>

#include <glm/glm.hpp>

namespace vm {

namespace detail {
struct ivec3_comparator {
    bool operator()(const glm::ivec3 &lhs, const glm::ivec3 &rhs) const {
        return std::tie(lhs.x, lhs.y, lhs.z) < std::tie(rhs.x, rhs.y, rhs.z);
    }
};
} // namespace detail

typedef std::set<glm::ivec3, detail::ivec3_comparator> CoordSet;

class SceneArchive {
    std::string m_workdir;
    std::mutex m_jobs_mutex;
    std::map<std::shared_ptr<Chunk>, Job> m_jobs;
    ThreadPool m_thread_pool;
    mutable CoordSet m_chunk_coords;

    compute::command_queue m_copy_queue;
    std::mutex m_queue_mutex;

    void discover_chunk_coords();
    std::string chunk_filename(const std::shared_ptr<Chunk> &chunk) const;

public:
    SceneArchive(const SceneArchive &) = delete;
    SceneArchive &operator=(const SceneArchive &) = delete;

    /** Creates / opens an archive at the specified directory */
    SceneArchive(const std::string &directory,
                 const std::shared_ptr<ComputeContext> &compute_ctx);
    ~SceneArchive();
    /** Gets the set of chunks available in the archive */
    const CoordSet &get_chunk_coords() const;
    void persist_later(std::shared_ptr<Chunk> chunk);
    void restore(std::shared_ptr<Chunk> chunk);
};

} // namespace vm

#endif /* VM_SCENE_SCENE_ARCHIVE_H */
