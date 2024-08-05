#pragma once

#include <functional>
#include <memory>
#include <unordered_set>


class Composable;

class Scene final {
public:
    [[nodiscard]] static std::shared_ptr<Scene> create();

    void insert(const std::shared_ptr<Composable>& composable);
    void remove(const std::shared_ptr<Composable>& composable);

    void forEach(const std::function<void(const std::shared_ptr<Composable>&)>& f) const;

private:
    std::unordered_set<std::shared_ptr<Composable>> _composables{};
};
