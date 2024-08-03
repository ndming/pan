#include "engine/Composable.h"

#include <algorithm>


void Composable::attach(const std::shared_ptr<Composable>& child) {
    // Before adding the child, we check against self attachment and multiple parents. Even with this verification,
    // it's still possible for the call site to create a composble tree containing cycles. When this happens, it
    // causes a severe recursive rendering and memory leak. We could in fact trace against the _parent member and
    // check for any circular dependency. However, this might add an extra overhead when we attach a child, causing
    // a ridiculous performance hit. Rather, given that the call site (in their right consciousness) would rarely form
    // a circular tree in practice, we will assumme such a mess would never happen.
    if (child.get() != this && !child->attached()) {
        child->_parent = shared_from_this();
        _children.insert(child);
    }
}

void Composable::detach(const std::shared_ptr<Composable>& child) noexcept {
    // Only detach when the child a descendant of this composable
    if (hasChild(child)) {
        child->_parent.reset();
        _children.erase(child);
    }
}

void Composable::attachAll(const std::initializer_list<std::shared_ptr<Composable>> children) {
    std::ranges::for_each(children, [this](const auto it) { attach(it); });
}

void Composable::detachAll() noexcept {
    std::ranges::for_each(_children, [this](const auto it) { detach(it); });
}

bool Composable::attached() const noexcept {
    return !_parent.expired();
}

bool Composable::hasChild(const std::shared_ptr<Composable>& child) const noexcept {
    return _children.contains(child);
}

Composable::~Composable() {
    if (attached()) {
        _parent.lock()->detach(shared_from_this());
    }
    detachAll();
}
