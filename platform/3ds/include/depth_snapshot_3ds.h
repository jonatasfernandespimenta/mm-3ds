#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>
#include <algorithm>
#include <vector>

#if defined(__3DS__) && defined(__arm__)
extern "C" void Soh3dsCopyWordsArm11(void* dst, const void* src, uint32_t words);
#endif

// CPU-owned copy of the content tiles in a completed PICA D24S8 buffer.
// Keep Morton order within each tile; omit unqueryable backing padding.
class DepthSnapshot3DS {
  public:
    bool requested = false;
    static constexpr uint16_t Far = 0xFFFC;

    void Reset() {
        requested = false;
        valid = false;
        std::vector<uint32_t>().swap(pixels);
        std::vector<uint8_t>().swap(tileValid);
        wanted.clear();
    }

    // MM-3DS: marks the 8x8 tile (and neighbours) of a query for the next
    // capture. The game queries 1-2 points (sun, glints); copying the whole
    // buffer (~375 KiB) every frame for that dominated the cost. With no
    // known geometry yet, the next capture is full.
    void Want(float x, float y) {
        requested = true;
        uint32_t tile;
        if (!valid) {
            wantAll = true;
            return;
        }
        if (!TileOf(x, y, &tile)) {
            return; // off-screen: Sample already returns Far
        }
        const int cols = static_cast<int>(capturedTileColumns);
        const int rows = static_cast<int>(capturedTileRows);
        const int cx = static_cast<int>(tile % capturedTileColumns), cy = static_cast<int>(tile / capturedTileColumns);
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (cx + dx >= 0 && cx + dx < cols && cy + dy >= 0 && cy + dy < rows) {
                    wanted.push_back(static_cast<uint32_t>((cy + dy) * cols + cx + dx));
                }
            }
        }
    }

    bool Capture(const void* source, uint32_t width, uint32_t height,
                 uint32_t contentWidth, uint32_t contentHeight, bool rotate) {
        const bool wasValid = valid;
        valid = false;
        if (!source || width == 0 || height == 0 || width > 1024 || height > 1024 ||
            width % 8 || height % 8 || contentWidth == 0 || contentHeight == 0 ||
            contentWidth > (rotate ? height : width) || contentHeight > (rotate ? width : height)) {
            wanted.clear();
            return false;
        }
        const uint32_t tileColumns = ((rotate ? contentHeight : contentWidth) + 7U) / 8U;
        const uint32_t firstTileRow = (height - (rotate ? contentWidth : contentHeight)) / 8U;
        const uint32_t tileRows = height / 8U - firstTileRow;
        const size_t rowPixels = static_cast<size_t>(tileColumns) * 64U;
        const size_t pixelCount = rowPixels * tileRows;
        const bool sameGeometry = wasValid && tileValid.size() == static_cast<size_t>(tileColumns) * tileRows &&
                                  capturedTileColumns == tileColumns && capturedFirstTileRow == firstTileRow &&
                                  backingHeight == height && drawWidth == contentWidth &&
                                  drawHeight == contentHeight && rotated == rotate;
        try {
            // Reuse stable-size captures without allocating. On growth, avoid
            // vector's geometric spare capacity on a memory-limited handheld.
            if (pixelCount > pixels.capacity()) {
                std::vector<uint32_t>(pixelCount).swap(pixels);
            } else {
                pixels.resize(pixelCount);
            }
            const size_t tileCount = static_cast<size_t>(tileColumns) * tileRows;
            if (tileCount > tileValid.capacity()) {
                std::vector<uint8_t>(tileCount).swap(tileValid);
            } else {
                tileValid.resize(tileCount);
            }
        } catch (const std::bad_alloc&) {
            return false;
        }
        const size_t sourceRowPixels = static_cast<size_t>(width) * 8U;
        const auto* sourceBytes = static_cast<const unsigned char*>(source) +
                                  firstTileRow * sourceRowPixels * sizeof(uint32_t);
        if (!wantAll && sameGeometry) {
            // Only the wanted tiles; the rest stays invalid (Sample returns Far).
            std::fill(tileValid.begin(), tileValid.end(), 0);
            for (uint32_t tile : wanted) {
                const uint32_t row = tile / tileColumns, col = tile % tileColumns;
                std::memcpy(pixels.data() + static_cast<size_t>(tile) * 64U,
                            sourceBytes + (row * sourceRowPixels + col * 64U) * sizeof(uint32_t),
                            64U * sizeof(uint32_t));
                tileValid[tile] = 1;
            }
            wanted.clear();
            valid = true;
            return true;
        }
        std::fill(tileValid.begin(), tileValid.end(), 1);
        wanted.clear();
        wantAll = false;
        if (rowPixels == sourceRowPixels) {
#if defined(__3DS__) && defined(__arm__)
            Soh3dsCopyWordsArm11(pixels.data(), sourceBytes, static_cast<uint32_t>(pixelCount));
#else
            std::memcpy(pixels.data(), sourceBytes, pixelCount * sizeof(uint32_t));
#endif
        } else {
            for (uint32_t row = 0; row < tileRows; ++row) {
#if defined(__3DS__) && defined(__arm__)
                Soh3dsCopyWordsArm11(pixels.data() + row * rowPixels,
                                     sourceBytes + row * sourceRowPixels * sizeof(uint32_t),
                                     static_cast<uint32_t>(rowPixels));
#else
                std::memcpy(pixels.data() + row * rowPixels,
                            sourceBytes + row * sourceRowPixels * sizeof(uint32_t),
                            rowPixels * sizeof(uint32_t));
#endif
            }
        }
        capturedTileColumns = tileColumns;
        capturedTileRows = tileRows;
        capturedFirstTileRow = firstTileRow;
        backingHeight = height;
        drawWidth = contentWidth;
        drawHeight = contentHeight;
        rotated = rotate;
        valid = true;
        return true;
    }

    uint16_t Sample(float x, float y) const {
        uint32_t tile;
        if (!TileOf(x, y, &tile) || tile >= tileValid.size() || !tileValid[tile]) {
            return Far;
        }
        // Same containing-pixel convention as GL's integer glReadPixels args.
        const uint32_t px = static_cast<uint32_t>(x * drawWidth / 400.f);
        const uint32_t py = static_cast<uint32_t>(y * drawHeight / 240.f);
        if (px >= drawWidth || py >= drawHeight) return Far;
        // Queries use GL window coordinates (Y up). The tilted projection
        // maps (x,y) to (y,-x); PICA memory then reverses rasterizer Y about
        // the full backing height, including any power-of-two padding.
        const uint32_t tx = rotated ? py : px;
        const uint32_t ty = rotated ? backingHeight - drawWidth + px : backingHeight - 1 - py;
        size_t morton = 0;
        for (unsigned bit = 0; bit < 3; ++bit) {
            morton |= ((tx >> bit) & 1U) << (2 * bit);
            morton |= ((ty >> bit) & 1U) << (2 * bit + 1);
        }
        const size_t offset = ((ty / 8U - capturedFirstTileRow) * capturedTileColumns + tx / 8U) * 64U + morton;
        // PICA uses reversed depth: zero is far; the upper byte is stencil.
        return static_cast<uint16_t>(((0xFFFFFFU - (pixels[offset] & 0xFFFFFFU)) >> 10U) << 2U);
    }

  private:
    // Index of the tile (captured row * columns + column) containing (x, y).
    bool TileOf(float x, float y, uint32_t* tile) const {
        if (!valid || !std::isfinite(x) || !std::isfinite(y) || x < 0 || y < 0 || x >= 400 || y >= 240) {
            return false;
        }
        const uint32_t px = static_cast<uint32_t>(x * drawWidth / 400.f);
        const uint32_t py = static_cast<uint32_t>(y * drawHeight / 240.f);
        if (px >= drawWidth || py >= drawHeight) return false;
        const uint32_t tx = rotated ? py : px;
        const uint32_t ty = rotated ? backingHeight - drawWidth + px : backingHeight - 1 - py;
        if (ty / 8U < capturedFirstTileRow) return false;
        *tile = (ty / 8U - capturedFirstTileRow) * capturedTileColumns + tx / 8U;
        return true;
    }

    std::vector<uint32_t> pixels;
    std::vector<uint8_t> tileValid;
    std::vector<uint32_t> wanted;
    bool wantAll = true;
    uint32_t capturedTileRows = 0;
    uint32_t capturedTileColumns = 0, capturedFirstTileRow = 0;
    uint32_t backingHeight = 0, drawWidth = 0, drawHeight = 0;
    bool valid = false, rotated = false;
};
