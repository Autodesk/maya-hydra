//
// Copyright 2026 Autodesk
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#include "gaussianSplatPlyReader.h"

#include <pxr/base/tf/stringUtils.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

namespace {

enum class _Type
{
    Int8,
    UInt8,
    Int16,
    UInt16,
    Int32,
    UInt32,
    Float32,
    Float64
};

bool _ParseType(const std::string& name, _Type& type, size_t& size)
{
    static const struct
    {
        const char* name;
        _Type       type;
        size_t      size;
    } types[] = {
        { "char", _Type::Int8, 1 },     { "int8", _Type::Int8, 1 },
        { "uchar", _Type::UInt8, 1 },   { "uint8", _Type::UInt8, 1 },
        { "short", _Type::Int16, 2 },   { "int16", _Type::Int16, 2 },
        { "ushort", _Type::UInt16, 2 }, { "uint16", _Type::UInt16, 2 },
        { "int", _Type::Int32, 4 },     { "int32", _Type::Int32, 4 },
        { "uint", _Type::UInt32, 4 },   { "uint32", _Type::UInt32, 4 },
        { "float", _Type::Float32, 4 }, { "float32", _Type::Float32, 4 },
        { "double", _Type::Float64, 8 }, { "float64", _Type::Float64, 8 },
    };
    for (const auto& t : types) {
        if (name == t.name) {
            type = t.type;
            size = t.size;
            return true;
        }
    }
    return false;
}

/// Reads a T from the \p available bytes at \p p; 0 when they are too few.
template <typename T> float _Load(const char* p, size_t available)
{
    if (available < sizeof(T)) {
        return 0.0f;
    }
    T value {};
    std::copy_n(p, sizeof(T), reinterpret_cast<char*>(&value));
    return static_cast<float>(value);
}

// The file is little-endian, as is every platform Maya runs on.
float _ReadFloat(const char* p, size_t available, _Type type)
{
    switch (type) {
    case _Type::Int8: return _Load<int8_t>(p, available);
    case _Type::UInt8: return _Load<uint8_t>(p, available);
    case _Type::Int16: return _Load<int16_t>(p, available);
    case _Type::UInt16: return _Load<uint16_t>(p, available);
    case _Type::Int32: return _Load<int32_t>(p, available);
    case _Type::UInt32: return _Load<uint32_t>(p, available);
    case _Type::Float32: return _Load<float>(p, available);
    case _Type::Float64: return _Load<double>(p, available);
    }
    return 0.0f;
}

struct _Property
{
    std::string name;
    _Type       type { _Type::Float32 };
    size_t      offset { 0 };
};

struct _Element
{
    std::string            name;
    uint64_t               count { 0 };
    std::vector<_Property> properties;
    size_t                 stride { 0 };
    bool                   hasList { false };
};

bool _ReadLine(std::istream& in, std::string& line)
{
    if (!std::getline(in, line)) {
        return false;
    }
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    return true;
}

bool _ReadHeader(std::istream& in, std::vector<_Element>& elements, std::string& error)
{
    std::string line;
    if (!_ReadLine(in, line) || line != "ply") {
        error = "not a PLY file";
        return false;
    }
    while (_ReadLine(in, line)) {
        std::istringstream words(line);
        std::string        keyword;
        words >> keyword;
        if (keyword == "end_header") {
            return true;
        }
        if (keyword == "format") {
            std::string format;
            words >> format;
            if (format != "binary_little_endian") {
                error = "unsupported PLY format '" + format + "' (only binary_little_endian)";
                return false;
            }
        } else if (keyword == "element") {
            _Element element;
            words >> element.name >> element.count;
            if (words.fail()) {
                error = "malformed element line: " + line;
                return false;
            }
            elements.push_back(std::move(element));
        } else if (keyword == "property") {
            if (elements.empty()) {
                error = "property declared before any element";
                return false;
            }
            _Element&   element = elements.back();
            std::string typeName;
            words >> typeName;
            if (typeName == "list") {
                element.hasList = true;
                continue;
            }
            _Property property;
            size_t    size = 0;
            words >> property.name;
            if (words.fail() || !_ParseType(typeName, property.type, size)) {
                error = "malformed property line: " + line;
                return false;
            }
            property.offset = element.stride;
            element.stride += size;
            element.properties.push_back(std::move(property));
        }
        // "comment" and "obj_info" lines are ignored.
    }
    error = "truncated PLY header";
    return false;
}

} // namespace

bool MayaHydraReadGaussianSplatPly(
    const std::string&       path,
    MayaHydraGaussianSplats& splats,
    std::string&             error)
{
    splats = {};

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "cannot open file";
        return false;
    }
    in.seekg(0, std::ios::end);
    const auto fileSize = static_cast<uint64_t>(in.tellg());
    in.seekg(0, std::ios::beg);

    std::vector<_Element> elements;
    if (!_ReadHeader(in, elements, error)) {
        return false;
    }

    // Skip the fixed-size elements stored before the vertices.
    const _Element* vertex = nullptr;
    for (const auto& element : elements) {
        if (element.name == "vertex") {
            vertex = &element;
            break;
        }
        if (element.hasList) {
            error = "variable-size element '" + element.name + "' precedes the vertices";
            return false;
        }
        in.seekg(static_cast<std::streamoff>(element.count * element.stride), std::ios::cur);
    }
    if (!vertex) {
        error = "no 'vertex' element";
        return false;
    }
    if (vertex->hasList) {
        error = "the 'vertex' element has list properties";
        return false;
    }

    // Order matters: the decoding loop below indexes this table.
    static const char* const required[] = {
        "x",     "y",     "z",     "scale_0", "scale_1", "scale_2", "rot_0",
        "rot_1", "rot_2", "rot_3", "opacity", "f_dc_0",  "f_dc_1",  "f_dc_2",
    };
    constexpr size_t kRequiredCount = sizeof(required) / sizeof(required[0]);
    const _Property* props[kRequiredCount] = {};
    for (size_t i = 0; i < kRequiredCount; ++i) {
        for (const auto& property : vertex->properties) {
            if (property.name == required[i]) {
                props[i] = &property;
                break;
            }
        }
        if (!props[i]) {
            error = TfStringPrintf("missing vertex property '%s'", required[i]);
            return false;
        }
    }

    const uint64_t dataStart = static_cast<uint64_t>(in.tellg());
    const uint64_t available = dataStart <= fileSize ? fileSize - dataStart : 0;
    const size_t   stride = vertex->stride;
    if (!in || vertex->count > available / stride) {
        error = TfStringPrintf(
            "file too short for %llu splats", static_cast<unsigned long long>(vertex->count));
        return false;
    }
    const auto n = static_cast<size_t>(vertex->count);

    splats.positions.resize(n);
    splats.scales.resize(n);
    splats.orientations.resize(n);
    splats.opacities.resize(n);
    splats.shDc.resize(n);
    GfVec3f* positions = splats.positions.data();
    GfVec3f* scales = splats.scales.data();
    GfQuatf* orientations = splats.orientations.data();
    float*   opacities = splats.opacities.data();
    GfVec3f* shDc = splats.shDc.data();

    constexpr size_t  kChunk = 64 * 1024;
    std::vector<char> buffer(std::min(n, kChunk) * stride);
    for (size_t first = 0; first < n; first += kChunk) {
        const size_t count = std::min(kChunk, n - first);
        if (!in.read(buffer.data(), static_cast<std::streamsize>(count * stride))) {
            error = TfStringPrintf("file ended after %zu of %zu splats", first, n);
            splats = {};
            return false;
        }
        for (size_t i = 0; i < count; ++i) {
            const char* v = buffer.data() + i * stride;
            float       f[kRequiredCount];
            for (size_t k = 0; k < kRequiredCount; ++k) {
                const size_t offset = props[k]->offset;
                f[k] = _ReadFloat(
                    v + offset, offset < stride ? stride - offset : 0, props[k]->type);
            }
            const size_t idx = first + i;
            positions[idx] = GfVec3f(f[0], f[1], f[2]);
            scales[idx] = GfVec3f(std::exp(f[3]), std::exp(f[4]), std::exp(f[5]));
            GfQuatf q(f[6], f[7], f[8], f[9]);
            q.Normalize();
            orientations[idx] = q;
            opacities[idx] = 1.0f / (1.0f + std::exp(-f[10]));
            shDc[idx] = GfVec3f(f[11], f[12], f[13]);
        }
    }
    return true;
}

PXR_NAMESPACE_CLOSE_SCOPE
