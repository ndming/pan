#pragma once

#include <engine/Overlay.h>


class GUI final : public Overlay {
public:
    void define() override;

private:
    bool _showDemoWindow{ true };
};
