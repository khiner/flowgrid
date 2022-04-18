#pragma once

#include "zep/display.h"
#include "zep/syntax.h"
#include <string>
#include "imgui.h"

namespace Zep {

inline NVec2f toNVec2f(const ImVec2 &im) { return {im.x, im.y}; }
inline ImVec2 toImVec2(const NVec2f &im) { return {im.x, im.y}; }

static ImWchar greek_range[] = {0x300, 0x52F, 0x1f00, 0x1fff, 0, 0};

struct ZepFont_ImGui : public ZepFont {
    ZepFont_ImGui(ZepDisplay &display, ImFont *pFont, int pixelHeight) : ZepFont(display), font(pFont) {
        SetPixelHeight(pixelHeight);
    }

    void SetPixelHeight(int pixelHeight) override {
        InvalidateCharCache();
        this->pixelHeight = pixelHeight;
    }

    NVec2f GetTextSize(const uint8_t *pBegin, const uint8_t *pEnd = nullptr) const override {
        // This is the code from ImGui internals; we can't call GetTextSize, because it doesn't return the correct 'advance' formula, which we
        // need as we draw one character at a time...
        ImVec2 text_size = font->CalcTextSizeA(float(pixelHeight), FLT_MAX, FLT_MAX, (const char *) pBegin, (const char *) pEnd, NULL);
        if (text_size.x == 0.0) {
            // Make invalid characters a default fixed_size
            const char chDefault = 'A';
            text_size = font->CalcTextSizeA(float(pixelHeight), FLT_MAX, FLT_MAX, &chDefault, (&chDefault + 1), NULL);
        }

        return toNVec2f(text_size);
    }

    ImFont *font;
};

struct ZepDisplay_ImGui : public ZepDisplay {
    ZepDisplay_ImGui() : ZepDisplay() {}

    void DrawChars(ZepFont &font, const NVec2f &pos, const NVec4f &col, const uint8_t *text_begin, const uint8_t *text_end) const override {
        auto imFont = dynamic_cast<ZepFont_ImGui &>(font).font;
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        if (text_end == nullptr) {
            text_end = text_begin + strlen((const char *) text_begin);
        }
        const auto modulatedColor = GetStyleModulatedColor(col);
        if (m_clipRect.Width() == 0) {
            drawList->AddText(imFont, float(font.pixelHeight), toImVec2(pos), modulatedColor, (const char *) text_begin, (const char *) text_end);
        } else {
            drawList->PushClipRect(toImVec2(m_clipRect.topLeftPx), toImVec2(m_clipRect.bottomRightPx));
            drawList->AddText(imFont, float(font.pixelHeight), toImVec2(pos), modulatedColor, (const char *) text_begin, (const char *) text_end);
            drawList->PopClipRect();
        }
    }

    void DrawLine(const NVec2f &start, const NVec2f &end, const NVec4f &color, float width) const override {
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        const auto modulatedColor = GetStyleModulatedColor(color);

        // Background rect for numbers
        if (m_clipRect.Width() == 0) {
            drawList->AddLine(toImVec2(start), toImVec2(end), modulatedColor, width);
        } else {
            drawList->PushClipRect(toImVec2(m_clipRect.topLeftPx), toImVec2(m_clipRect.bottomRightPx));
            drawList->AddLine(toImVec2(start), toImVec2(end), modulatedColor, width);
            drawList->PopClipRect();
        }
    }

    void DrawRectFilled(const NRectf &rc, const NVec4f &color) const override {
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        const auto modulatedColor = GetStyleModulatedColor(color);
        // Background rect for numbers
        if (m_clipRect.Width() == 0) {
            drawList->AddRectFilled(toImVec2(rc.topLeftPx), toImVec2(rc.bottomRightPx), modulatedColor);
        } else {
            drawList->PushClipRect(toImVec2(m_clipRect.topLeftPx), toImVec2(m_clipRect.bottomRightPx));
            drawList->AddRectFilled(toImVec2(rc.topLeftPx), toImVec2(rc.bottomRightPx), modulatedColor);
            drawList->PopClipRect();
        }
    }

    void SetClipRect(const NRectf &rc) override { m_clipRect = rc; }

    ZepFont &GetFont(ZepTextType type) override {
        if (fonts[(int) type] == nullptr) {
            fonts[(int) type] = std::make_shared<ZepFont_ImGui>(*this, ImGui::GetIO().Fonts[0].Fonts[0], int(16.0f * pixelScale.y));
        }
        return *fonts[(int) type];
    }

private:
    static ImU32 GetStyleModulatedColor(const NVec4f &color) {
        return ToPackedABGR(NVec4f(color.x, color.y, color.z, color.w * ImGui::GetStyle().Alpha));
    }

    NRectf m_clipRect;
}; // namespace Zep

} // namespace Zep
