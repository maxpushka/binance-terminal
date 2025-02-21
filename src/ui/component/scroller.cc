// Copyright 2021 Arthur Sonzogni.
module;
#include <algorithm>  // for max, min
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <memory>  // for shared_ptr, allocator, __shared_ptr_access

#include "ftxui/component/component.hpp"
#include "ftxui/component/mouse.hpp"
#include "ftxui/dom/deprecated.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/dom/node.hpp"
#include "ftxui/dom/requirement.hpp"
#include "ftxui/screen/box.hpp"

module ui.component;

namespace component {

using namespace ftxui;
class ScrollerBase : public ComponentBase {
 public:
  ScrollerBase(Component child) { Add(child); }

 private:
  Element Render() final {
    const auto focused = Focused() ? focus : ftxui::select;
    const auto style = Focused() ? inverted : nothing;

    Element background = ComponentBase::Render();
    background->ComputeRequirement();
    size_ = background->requirement().min_y;
    return dbox({
               std::move(background),
               vbox({
                   text(L"") | size(HEIGHT, EQUAL, selected_),
                   text(L"") | style | focused,
               }),
           }) |
           vscroll_indicator | yframe | yflex | reflect(box_);
  }

  bool OnEvent(Event event) final {
    if (event.is_mouse() && box_.Contain(event.mouse().x, event.mouse().y))
      TakeFocus();

    int selected_old = selected_;
    if (event == Event::ArrowUp || event == Event::Character('k') ||
        (event.is_mouse() && event.mouse().button == Mouse::WheelUp)) {
      selected_--;
    }
    if ((event == Event::ArrowDown || event == Event::Character('j') ||
         (event.is_mouse() && event.mouse().button == Mouse::WheelDown))) {
      selected_++;
    }
    if (event == Event::PageDown) selected_ += box_.y_max - box_.y_min;
    if (event == Event::PageUp) selected_ -= box_.y_max - box_.y_min;
    if (event == Event::Home) selected_ = 0;
    if (event == Event::End) selected_ = size_;

    selected_ = std::max(0, std::min(size_ - 1, selected_));
    return selected_old != selected_;
  }

  [[nodiscard]] bool Focusable() const final { return true; }

  int selected_ = 0;
  int size_ = 0;
  Box box_;
};

Component Scroller(Component child) {
  return Make<ScrollerBase>(std::move(child));
}
}  // namespace component
