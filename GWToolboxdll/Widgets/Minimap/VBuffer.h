#pragma once

#include "D3DVertex.h"

/*
This class is essentially a glorified vertex buffer, containing everything
that is necessary to render the vertex buffer.

classes implementing this class only need to implement Initialize which
should contain code that:
- populates the vertex buffer "buffer_"
- sets the primitive type "type_"
- sets the primitive count "count_"

afterward just call Render and the vertex buffer will be rendered

you can call Invalidate() to have the initialize be called again on render
*/

class VBuffer {
public:
    virtual ~VBuffer() = default;

    virtual void Invalidate()
    {
        if (buffer) {
            buffer->Release();
        }
        buffer = nullptr;
        initialized = false;
    }

    virtual void Render(IDirect3DDevice9* device)
    {
        RenderVertices(device, 0, count);
    }

    void RenderVertices(IDirect3DDevice9* device, size_t offset, size_t _count)
    {
        if (!initialized) {
            initialized = true;
            Initialize(device);
        }
        ASSERT(offset + _count <= count);
        device->SetFVF(D3DFVF_CUSTOMVERTEX);
        device->SetStreamSource(0, buffer, 0, sizeof(D3DVertex));
        device->DrawPrimitive(type, 0, _count);
    }

    virtual void Terminate()
    {
        Invalidate();
    }

    virtual void Initialize(IDirect3DDevice9* device) = 0;

protected:
    IDirect3DVertexBuffer9* buffer = nullptr;
    D3DPRIMITIVETYPE type = D3DPT_TRIANGLELIST;
    unsigned long count = 0;
    bool initialized = false;
};
