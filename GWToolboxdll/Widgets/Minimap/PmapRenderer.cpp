#include "stdafx.h"

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameEntities/Pathing.h>

#include <GWCA/Managers/MapMgr.h>

#include <Widgets/Minimap/D3DVertex.h>
#include <Widgets/Minimap/PmapRenderer.h>

#include <ImGuiAddons.h>

#include "Minimap.h"

namespace {
    void SetDeviceColor(IDirect3DDevice9* device, D3DCOLOR color)
    {
        device->SetRenderState(D3DRS_TEXTUREFACTOR, color);
        device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
        device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
    }

    void ResetDeviceColor(IDirect3DDevice9* device)
    {
        device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    }

    void SetDeviceTranslation(IDirect3DDevice9* device, float x, float y, float z, D3DMATRIX* old_view = nullptr)
    {
        D3DMATRIX current;
        device->GetTransform(D3DTS_VIEW, &current);
        if (old_view) *old_view = current;

        const auto matrix = XMLoadFloat4x4(reinterpret_cast<const DirectX::XMFLOAT4X4*>(&current));
        const auto translated = matrix * DirectX::XMMatrixTranslation(x, y, z);
        device->SetTransform(D3DTS_VIEW, reinterpret_cast<const D3DMATRIX*>(&translated));
    }

    void ResetDeviceTranslation(IDirect3DDevice9* device, const D3DMATRIX& old_view)
    {
        device->SetTransform(D3DTS_VIEW, &old_view);
    }
}


void PmapRenderer::DrawSettings() {}

void PmapRenderer::Initialize(IDirect3DDevice9* device)
{
    //#define WIREFRAME_MODE

    GW::PathingMapArray* path_map;
    if (GW::Map::GetIsMapLoaded()) {
        path_map = GW::Map::GetPathingMap();
    }
    else {
        initialized = false;
        return; // no map loaded yet, so don't render anything
    }

    // get the number of trapezoids, need it to allocate the vertex buffer
    trapez_count_ = 0;
    for (const GW::PathingMap& map : *path_map) {
        trapez_count_ += map.trapezoid_count;
    }
    if (trapez_count_ == 0) {
        return;
    }

#ifdef WIREFRAME_MODE

    total_tri_count_ = tri_count_ = trapez_count_ * 4;

    vert_count_ = tri_count_ * 2;
    count = total_tri_count_ * 2;

#else

    total_tri_count_ = tri_count_ = trapez_count_ * 2;

    vert_count_ = tri_count_ * 3;
    count = total_tri_count_ * 3;

#endif

    D3DVertex* vertices = nullptr;

    // allocate new vertex buffer
    if (buffer) {
        buffer->Release();
    }
    device->CreateVertexBuffer(sizeof(D3DVertex) * count, D3DUSAGE_WRITEONLY,
                               D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, nullptr);
    buffer->Lock(0, sizeof(D3DVertex) * count,
                 reinterpret_cast<void**>(&vertices), D3DLOCK_DISCARD);

    D3DVertex* vertices_begin = vertices;

#ifdef WIREFRAME_MODE
    type = D3DPT_LINELIST;

    // populate vertex buffer
    for (size_t i = 0; i < path_map.size(); i++) {
        GW::PathingMap pmap = path_map[i];
        for (size_t j = 0; j < pmap.trapezoid_count; ++j) {
            GW::PathingTrapezoid& trap = pmap.trapezoids[j];
            vertices[0].x = trap.XTL;
            vertices[0].y = trap.YT;
            vertices[1].x = trap.XTR;
            vertices[1].y = trap.YT;

            vertices[2].x = trap.XTR;
            vertices[2].y = trap.YT;
            vertices[3].x = trap.XBR;
            vertices[3].y = trap.YB;

            vertices[4].x = trap.XBR;
            vertices[4].y = trap.YB;
            vertices[5].x = trap.XBL;
            vertices[5].y = trap.YB;

            vertices[6].x = trap.XBL;
            vertices[6].y = trap.YB;
            vertices[7].x = trap.XTL;
            vertices[7].y = trap.YT;

            vertices += 8;
        }
    }
#else
    type = D3DPT_TRIANGLELIST;

    // populate vertex buffer
    for (const GW::PathingMap& pmap : *path_map) {
        for (size_t j = 0; j < pmap.trapezoid_count; ++j) {
            GW::PathingTrapezoid& trap = pmap.trapezoids[j];

            vertices[0].x = trap.XTL;
            vertices[0].y = trap.YT;
            vertices[1].x = trap.XTR;
            vertices[1].y = trap.YT;
            vertices[2].x = trap.XBL;
            vertices[2].y = trap.YB;

            vertices[3].x = trap.XBL;
            vertices[3].y = trap.YB;
            vertices[4].x = trap.XTR;
            vertices[4].y = trap.YT;
            vertices[5].x = trap.XBR;
            vertices[5].y = trap.YB;
            vertices += 6;
        }
    }
#endif

    vertices = vertices_begin;
    for (auto i = 0u; i < count; i++) {
        vertices[i].z = 0.0f;
    }

    buffer->Unlock();
}

void PmapRenderer::Render(IDirect3DDevice9* device, const MinimapRenderContext& ctx)
{
    if (!initialized) {
        initialized = true;
        Initialize(device);
    }

    if (ctx.shadow_color & IM_COL32_A_MASK) {
        D3DMATRIX oldview;
        SetDeviceTranslation(device, 0, -100.f, 0.f, &oldview);

        SetDeviceColor(device, ctx.shadow_color);
        RenderVertices(device, 0, tri_count_);
        ResetDeviceTranslation(device, oldview);
        ResetDeviceColor(device);
    }
    if (ctx.foreground_color & IM_COL32_A_MASK) {
        SetDeviceColor(device, ctx.foreground_color);
        RenderVertices(device, 0, tri_count_);
        ResetDeviceColor(device);
    }
}
