#include "config.h"

#include "scene-archive.h"
#include "utils/log.h"
#include "utils/persistence.h"

#include <cassert>

#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/write.hpp>
#include <boost/iostreams/read.hpp>

using namespace std;
using namespace glm;
namespace fs = boost::filesystem;

namespace vm {

static const uint16_t ARCHIVE_VERSION = 1;

struct ArchiveHeader {
    uint16_t version;
    uint16_t chunk_size;
    uint16_t chunk_border;
    double voxel_size;
};

namespace detail {

static void validate_header(fstream &file) {
    ArchiveHeader header{};
    file >= header.version;
    file >= header.chunk_size;
    file >= header.chunk_border;
    file >= header.voxel_size;

    if (header.version != ARCHIVE_VERSION) {
        throw runtime_error(
                boost::str(boost::format("Expected version %1%, got: %2%")
                           % ARCHIVE_VERSION % header.version));
    }
    if (header.chunk_size != VM_CHUNK_SIZE) {
        throw runtime_error(
                boost::str(boost::format("Expected chunk size %1%, got: %2%")
                           % VM_CHUNK_SIZE % header.chunk_size));
    }
    if (header.chunk_border != VM_CHUNK_BORDER) {
        throw runtime_error(
                boost::str(boost::format("Expected chunk border %1%, got: %2%")
                           % VM_CHUNK_BORDER % header.chunk_border));
    }
    if (header.voxel_size != VM_VOXEL_SIZE) {
        throw runtime_error(
                boost::str(boost::format("Expected voxel size %1%, got: %2%")
                           % VM_VOXEL_SIZE % header.voxel_size));
    }
}

static void write_header(ofstream &file) {
    // clang-format off
    file <= static_cast<uint16_t>(ARCHIVE_VERSION)
         <= static_cast<uint16_t>(VM_CHUNK_SIZE)
         <= static_cast<uint16_t>(VM_CHUNK_BORDER)
         <= static_cast<double>(VM_VOXEL_SIZE);
    // clang-format on
}

static void persist(const string &filename, const vector<uint8_t> &data) {
    using namespace boost::iostreams;
    ofstream file;
    file.exceptions(ofstream::failbit | ofstream::badbit);
    file.open(filename, ofstream::out | ofstream::binary);
    write_header(file);
    filtering_streambuf<output> out;
    out.push(zlib_compressor());
    out.push(file);
    boost::iostreams::write(out, reinterpret_cast<const char *>(data.data()),
                            data.size());

    LOG(trace) << "Persisted " << filename;
}

static vector<uint8_t> restore(const string &filename, size_t voxel_size) {
    using namespace boost::iostreams;
    fstream file;
    file.exceptions(fstream::failbit | fstream::badbit);
    file.open(filename, fstream::in | fstream::binary);
    validate_header(file);
    filtering_streambuf<input> in;
    in.push(zlib_decompressor());
    in.push(file);

    const size_t N = VM_CHUNK_SIZE + VM_CHUNK_BORDER;
    vector<uint8_t> data(voxel_size * N * N * N);
    boost::iostreams::read(in, reinterpret_cast<char *>(&data[0]), data.size());

    LOG(trace) << "Restored " << filename;
    return data;
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
            fs::path(detail::name_for_coord(chunk->get_coord())))
            .string();
}

SceneArchive::SceneArchive(const string &directory,
                           const shared_ptr<ComputeContext> &compute_ctx,
                           const compute::image_format &volume_format)
        : m_workdir(directory),
          // TODO
          m_voxel_size(2 * sizeof(uint16_t)),
          m_jobs_mutex(),
          m_jobs(),
          m_compute_ctx(compute_ctx),
          m_volume_format(volume_format),
          m_thread_pool(2) {
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

    m_jobs[chunk] = m_thread_pool.enqueue([=, &jobs_mutex=m_jobs_mutex]() {
        const size_t N = VM_CHUNK_SIZE + VM_CHUNK_BORDER;
        compute::event copy_finished;
        compute::event read_finished;
        compute::image3d image_copy(m_compute_ctx->context, N, N, N,
                                    m_volume_format);
        {
            lock_guard<mutex> chunk_lock(chunk->get_mutex());
            lock_guard<mutex> queue_lock(m_compute_ctx->queue_mutex);
            copy_finished = m_compute_ctx->queue.enqueue_copy_image(
                                                    chunk->get_volume(),
                                                    image_copy,
                                                    compute::dim(0,0,0).data(),
                                                    compute::dim(0,0,0).data(),
                                                    compute::dim(N,N,N).data());
        }
        copy_finished.wait();

        vector<uint8_t> buffer(m_voxel_size * N * N * N);
        {
            lock_guard<mutex> queue_lock(m_compute_ctx->queue_mutex);
            // NOTE: boost::compute does not have enqueue_read_image_async
            cl_int retval = clEnqueueReadImage(
                    m_compute_ctx->queue.get(),
                    image_copy.get(),
                    CL_FALSE,
                    compute::dim(0,0,0).data(),
                    compute::dim(N,N,N).data(),
                    0, 0, buffer.data(), 0, nullptr,
                    &read_finished.get());
            if (retval != CL_SUCCESS) {
                LOG(error) << "clEnqueueReadImage failed " << retval;
            }
            assert(retval == CL_SUCCESS);
        }
        read_finished.wait();

        detail::persist(chunk_filename(chunk), buffer);
        lock_guard<mutex> jobs_lock(jobs_mutex);
        m_jobs.erase(chunk);
    });
}

void SceneArchive::restore(shared_ptr<Chunk> chunk) {
    auto data = detail::restore(chunk_filename(chunk), m_voxel_size);
    const size_t N = VM_CHUNK_SIZE + VM_CHUNK_BORDER;
    m_compute_ctx->queue.enqueue_write_image<3>(chunk->get_volume(),
                                                compute::dim(0, 0, 0),
                                                compute::dim(N, N, N),
                                                data.data());
    m_compute_ctx->queue.flush();
    m_compute_ctx->queue.finish();
}

} // namespace vm
