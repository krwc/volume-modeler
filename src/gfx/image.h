#ifndef VM_GFX_IMAGE_H
#define VM_GFX_IMAGE_H

#define STB_IMAGE_IMPLEMENTATION
extern "C" {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#pragma GCC diagnostic ignored "-Wshift-negative-value"
#include "stb/stb_image.h"
#pragma GCC diagnostic pop
}

#include <stdexcept>
#include <string>

namespace vm {

struct Image {
    int width;
    int height;
    uint8_t *pixels;

    Image(const Image &) = delete;
    Image &operator=(const Image &) = delete;

    Image() : width(0), height(0), pixels(nullptr) {}

    Image(const std::string &path) : Image() {
        int components;
        pixels = stbi_load(path.c_str(), &width, &height, &components, 3);
        (void) components;
        if (!pixels) {
            throw std::runtime_error("Cannot load image from file: " + path);
        }
        assert(components == 3);
    }

    Image(Image &&other) : Image() {
        *this = std::move(other);
    }

    Image &operator=(Image &&other) {
        if (&other != this) {
            this->~Image();
            width = other.width;
            height = other.height;
            pixels = other.pixels;
            new (&other) Image();
        }
        return *this;
    }

    ~Image() {
        stbi_image_free(pixels);
    }
};

} // namespace vm

#endif /* VM_GFX_IMAGE_H */
