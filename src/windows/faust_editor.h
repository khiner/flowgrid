#pragma once

#include <filesystem>
#include "zep.h"
#include "drawable.h"

namespace fs = std::filesystem;

struct FaustEditor : public Drawable {
    void draw() override;
    void destroy() override;

private:
    void zep_init(const Zep::NVec2f &pixelScale);
    void zep_load(const Zep::ZepPath &file);

    struct ZepWrapper : public Zep::IZepComponent {
        ZepWrapper(const fs::path &root_path, const Zep::NVec2f &pixelScale, std::function<void(std::shared_ptr<Zep::ZepMessage>)> fnCommandCB)
            : zepEditor(Zep::ZepPath(root_path.string()), pixelScale), Callback(std::move(fnCommandCB)) {
            zepEditor.RegisterCallback(this);
        }

        Zep::ZepEditor &GetEditor() const override {
            return (Zep::ZepEditor &) zepEditor;
        }

        void Notify(std::shared_ptr<Zep::ZepMessage> message) override {
            Callback(message);
        }

        virtual void HandleInput() {
            zepEditor.HandleInput();
        }

        Zep::ZepEditor_ImGui zepEditor;
        std::function<void(std::shared_ptr<Zep::ZepMessage>)> Callback;
    };

    std::unique_ptr<ZepWrapper> spZep;
    bool initialized{false};
};
