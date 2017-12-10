#include "config.h"

#include "scene-archive.h"
#include "scene.h"
#include "utils/log.h"
#include "utils/persistence.h"

#include "compute/utils.h"

#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/read.hpp>
#include <boost/iostreams/write.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>

using namespace std;
using namespace glm;
namespace fs = boost::filesystem;

namespace vm {

static const uint16_t ARCHIVE_VERSION = 3;

struct ArchiveHeader {
    uint16_t version;
    uint16_t chunk_size;
    uint16_t chunk_border;
    double voxel_size;
    uint16_t edge_size;
    uint16_t sample_size;
};

namespace detail {

static void validate_header(fstream &file) {
    ArchiveHeader header{};
    file >= header.version;
    file >= header.chunk_size;
    file >= header.voxel_size;
    file >= header.edge_size;
    file >= header.sample_size;

    if (header.version != ARCHIVE_VERSION) {
        throw runtime_error(
                boost::str(boost::format("Expected version %1%, got: %2%")
                           % ARCHIVE_VERSION
                           % header.version));
    }
    if (header.chunk_size != VM_CHUNK_SIZE) {
        throw runtime_error(
                boost::str(boost::format("Expected chunk size %1%, got: %2%")
                           % VM_CHUNK_SIZE
                           % header.chunk_size));
    }
    if (header.voxel_size != VM_VOXEL_SIZE) {
        throw runtime_error(
                boost::str(boost::format("Expected voxel size %1%, got: %2%")
                           % VM_VOXEL_SIZE
                           % header.voxel_size));
    }
    if (header.edge_size != image_format_size(Scene::edges_format())) {
        throw runtime_error(
                boost::str(boost::format("Expected edge size %1%, got: %2%")
                           % image_format_size(Scene::edges_format())
                           % header.edge_size));
    }
    if (header.sample_size != image_format_size(Scene::samples_format())) {
        throw runtime_error(
                boost::str(boost::format("Expected sample size %1%, got: %2%")
                           % image_format_size(Scene::samples_format())
                           % header.sample_size));
    }
}

static void write_header(ofstream &file) {
    // clang-format off
    file <= static_cast<uint16_t>(ARCHIVE_VERSION)
         <= static_cast<uint16_t>(VM_CHUNK_SIZE)
         <= static_cast<double>(VM_VOXEL_SIZE)
         <= static_cast<uint16_t>(image_format_size(Scene::edges_format()))
         <= static_cast<uint16_t>(image_format_size(Scene::samples_format()));
    // clang-format on
}

static string name_for_coord(const ivec3 &coord) {
    return boost::str(boost::format("chunk_%1%_%2%_%3%.gz") % coord.x % coord.y
                      % coord.z);
}

} // namespace detail

void SceneArchive::discover_chunk_coords() {
    m_chunk_coords.clear();
    for (const auto &entry : fs::directory_iterator(m_workdir)) {
        if (!fs::is_regular_file(entry)) {
            continue;
        }
        string filename = entry.path().filename().string();
        boost::regex re("chunk_(-?\\d+)_(-?\\d+)_(-?\\d+)\\.gz");
        boost::smatch match;
        if (!boost::regex_match(filename, match, re)) {
            continue;
        }
        m_chunk_coords.emplace(boost::lexical_cast<int>(match[1]),
                               boost::lexical_cast<int>(match[2]),
                               boost::lexical_cast<int>(match[3]));
    }
}

string SceneArchive::chunk_filename(const shared_ptr<Chunk> &chunk) const {
    return (fs::path(m_workdir) /=
            fs::path(detail::name_for_coord(chunk->coord)))
            .string();
}

SceneArchive::SceneArchive(const string &directory,
                           const shared_ptr<ComputeContext> &compute_ctx)
        : m_workdir(directory)
        , m_jobs_mutex()
        , m_jobs()
        , m_thread_pool(2)
        , m_copy_queue(compute_ctx->make_out_of_order_queue())
        , m_queue_mutex() {
    if (!fs::exists(m_workdir)) {
        fs::create_directory(m_workdir);
    } else if (!fs::is_directory(m_workdir)) {
        throw invalid_argument(directory + " is not a directory");
    }
    discover_chunk_coords();
}

SceneArchive::~SceneArchive() {
    m_thread_pool.terminate();
}

const CoordSet &SceneArchive::get_chunk_coords() const {
    return m_chunk_coords;
}

void SceneArchive::persist_later(shared_ptr<Chunk> chunk) {
    lock_guard<mutex> jobs_lock(m_jobs_mutex);
    if (m_jobs.find(chunk) != m_jobs.end()) {
        m_thread_pool.cancel(m_jobs.at(chunk));
    }

    m_jobs[chunk] = m_thread_pool.enqueue([
        =,
        &jobs_mutex = m_jobs_mutex,
        &queue_mutex = m_queue_mutex
    ]() {
        const size_t N = VM_CHUNK_SIZE;

        vector<uint8_t> samples(image_format_size(Scene::samples_format())
                                * (N + 3) * (N + 3) * (N + 3));
        vector<uint8_t> edges_x(image_format_size(Scene::edges_format())
                                * (N + 2) * (N + 3) * (N + 3));
        vector<uint8_t> edges_y(image_format_size(Scene::edges_format())
                                * (N + 3) * (N + 2) * (N + 3));
        vector<uint8_t> edges_z(image_format_size(Scene::edges_format())
                                * (N + 3) * (N + 3) * (N + 2));
        {
            lock_guard<mutex> queue_lock(queue_mutex);
            lock_guard<mutex> chunk_lock(chunk->mutex);
            enqueue_read_image3d_async(
                    m_copy_queue, chunk->samples, samples.data());
            enqueue_read_image3d_async(
                    m_copy_queue, chunk->edges_x, edges_x.data());
            enqueue_read_image3d_async(
                    m_copy_queue, chunk->edges_y, edges_y.data());
            enqueue_read_image3d_async(
                    m_copy_queue, chunk->edges_z, edges_z.data());
            m_copy_queue.finish();
        }

        using namespace boost::iostreams;
        ofstream file;
        file.exceptions(ofstream::failbit | ofstream::badbit);
        file.open(chunk_filename(chunk), ofstream::out | ofstream::binary);
        detail::write_header(file);
        filtering_streambuf<output> out;
        out.push(zlib_compressor());
        out.push(file);
        boost::iostreams::write(out,
                                reinterpret_cast<const char *>(samples.data()),
                                samples.size());
        boost::iostreams::write(out,
                                reinterpret_cast<const char *>(edges_x.data()),
                                edges_x.size());
        boost::iostreams::write(out,
                                reinterpret_cast<const char *>(edges_y.data()),
                                edges_y.size());
        boost::iostreams::write(out,
                                reinterpret_cast<const char *>(edges_z.data()),
                                edges_z.size());

        lock_guard<mutex> jobs_lock(jobs_mutex);
        m_jobs.erase(chunk);

        LOG(trace) << "Persisted " << chunk_filename(chunk);
    });
    (void) chunk;
}

void SceneArchive::restore(shared_ptr<Chunk> chunk) {
    using namespace boost::iostreams;
    fstream file;
    file.exceptions(fstream::failbit | fstream::badbit);
    file.open(chunk_filename(chunk), fstream::in | fstream::binary);
    detail::validate_header(file);
    filtering_streambuf<input> in;
    in.push(zlib_decompressor());
    in.push(file);

    const size_t N = VM_CHUNK_SIZE;
    vector<uint8_t> samples(image_format_size(Scene::samples_format()) * (N + 3)
                            * (N + 3) * (N + 3));
    vector<uint8_t> edges_x(image_format_size(Scene::edges_format()) * (N + 2)
                            * (N + 3) * (N + 3));
    vector<uint8_t> edges_y(image_format_size(Scene::edges_format()) * (N + 3)
                            * (N + 2) * (N + 3));
    vector<uint8_t> edges_z(image_format_size(Scene::edges_format()) * (N + 3)
                            * (N + 3) * (N + 2));

    boost::iostreams::read(
            in, reinterpret_cast<char *>(&samples[0]), samples.size());
    boost::iostreams::read(
            in, reinterpret_cast<char *>(&edges_x[0]), edges_x.size());
    boost::iostreams::read(
            in, reinterpret_cast<char *>(&edges_y[0]), edges_y.size());
    boost::iostreams::read(
            in, reinterpret_cast<char *>(&edges_z[0]), edges_z.size());

    lock_guard<mutex> queue_lock(m_queue_mutex);
    lock_guard<mutex> chunk_lock(chunk->mutex);
    enqueue_write_image3d(m_copy_queue, chunk->samples, samples.data());
    enqueue_write_image3d(m_copy_queue, chunk->edges_x, edges_x.data());
    enqueue_write_image3d(m_copy_queue, chunk->edges_y, edges_y.data());
    enqueue_write_image3d(m_copy_queue, chunk->edges_z, edges_z.data());
    m_copy_queue.finish();

    LOG(trace) << "Restored " << chunk_filename(chunk);
}

} // namespace vm
