#include <mbgl/gl/vertex_array.hpp>
#include <mbgl/gl/index_buffer_resource.hpp>
#include <mbgl/gl/context.hpp>
#include <mbgl/platform/gl_functions.hpp>

namespace mln {
namespace gl {

using namespace platform;

void VertexArray::bind(Context& context,
                       const gfx::IndexBuffer& indexBuffer,
                       const AttributeBindingArray& bindings,
                       const AttributeBindingArray& instanceBindings) {
    context.bindVertexArray = state->vertexArray;
    state->indexBuffer = indexBuffer.getResource<gl::IndexBufferResource>().buffer;

    state->bindings.reserve(bindings.size());

    // NOLINTNEXTLINE(bugprone-too-small-loop-variable)
    for (AttributeLocation location = 0; location < bindings.size(); ++location) {
        if (state->bindings.size() <= location) {
            AttributeLocation loc = location;
            state->bindings.emplace_back(context, std::move(loc));
        }
        state->bindings[location] = bindings[location];
    }

    // Same as above, but the buffer advances once per instance instead of once per vertex.
    // glVertexAttribDivisor is part of this VAO's state (like glVertexAttribPointer above), so it
    // only needs setting once here at VAO-build time, not on every draw.
    state->bindings.reserve(std::max(state->bindings.size(), instanceBindings.size()));
    // NOLINTNEXTLINE(bugprone-too-small-loop-variable)
    for (AttributeLocation location = 0; location < instanceBindings.size(); ++location) {
        if (!instanceBindings[location]) {
            continue;
        }
        if (state->bindings.size() <= location) {
            AttributeLocation loc = location;
            state->bindings.emplace_back(context, std::move(loc));
        }
        state->bindings[location] = instanceBindings[location];
        MBGL_CHECK_ERROR(glVertexAttribDivisor(location, 1));
    }
}

} // namespace gl
} // namespace mln
