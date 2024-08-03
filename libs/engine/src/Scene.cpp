#include "engine/Scene.h"

#include <algorithm>


std::unique_ptr<Scene> Scene::create() {
    return std::unique_ptr<Scene>{ new Scene() };
}

void Scene::insert(const std::shared_ptr<Composable>& composable) {
    _composables.insert(composable);
}

void Scene::remove(const std::shared_ptr<Composable>& composable) {
    _composables.erase(composable);
}

void Scene::forEach(const std::function<void(const std::shared_ptr<Composable>&)>& f) const {
    std::ranges::for_each(_composables, f);
}
