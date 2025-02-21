module;
#include "ftxui/dom/table.hpp"

#include <mutex>
#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "rpp/subjects/publish_subject.hpp"

module ui.component;

namespace component {
using namespace ftxui;

TableHeader::TableHeader(
    const rpp::subjects::publish_subject<HeaderType>& header_input,
    rpp::subjects::publish_subject<RedrawSignal>& output)
    : header_update_subject_(output) {
  // Set a default header row.
  header_row_ = {"Column1", "Column2", "Column3"};
  header_input.get_observable().subscribe([this](const HeaderType& new_header) {
    header_row_ = new_header;
    header_update_subject_.get_observer().on_next(RedrawSignal{});
  });
}

void TableHeader::setColumnWidths(const std::vector<size_t>& new_widths) {
  if (new_widths == col_widths_) return;  // to avoid infinite cycle of redraws
  col_widths_ = new_widths;
  header_update_subject_.get_observer().on_next(RedrawSignal{});
}

Element TableHeader::RenderCell(const std::string& cell, const size_t width,
                                const size_t /*index*/) {
  return text(cell) | size(WIDTH, EQUAL, static_cast<int>(width)) | bold |
         color(Color::White);
}

Element TableHeader::Render() {
  std::vector<Element> cells;
  cells.reserve(header_row_.size());
  for (size_t i = 0; i < header_row_.size(); ++i) {
    const size_t width = i < col_widths_.size() ? col_widths_[i] : 10;
    cells.push_back(RenderCell(header_row_[i], width, i));
  }
  Table header_table({cells});
  return header_table.Render() | bgcolor(Color::Black);
}

template <typename T>
TableBody<T>::TableBody(const rpp::subjects::publish_subject<T>& event_input,
                        rpp::subjects::publish_subject<RedrawSignal>& output)
    : update_subject_(output) {
  event_input.get_observable().subscribe([this](const T& event) {
    ProcessEvent(event);
    update_subject_.get_observer().on_next(RedrawSignal{});
  });
}

template <typename T>
void TableBody<T>::ProcessEvent(const T& event) {
  std::lock_guard lock(mutex_);
  events_.push_back(event);
  // Keep only the latest 25 events.
  while (events_.size() > 25) events_.erase(events_.begin());
}

template <typename T>
Element TableBody<T>::RenderCell(const std::string& cell, const size_t width,
                                 const size_t col_index, const T& event) const {
  return text(cell) | size(WIDTH, EQUAL, static_cast<int>(width)) |
         color(Color::White);
}

template <typename T>
std::vector<std::vector<std::string>> TableBody<T>::getRows() const {
  std::lock_guard lock(mutex_);
  std::vector<std::vector<std::string>> rows;
  rows.reserve(events_.size());
  for (auto it = events_.rbegin(); it != events_.rend(); ++it) {
    rows.push_back(BuildRow(*it));
  }
  return rows;
}

template <typename T>
void TableBody<T>::setColumnWidths(const std::vector<size_t>& new_widths) {
  if (new_widths == col_widths_) return;  // to avoid infinite cycle of redraws
  col_widths_ = new_widths;
  update_subject_.get_observer().on_next(RedrawSignal{});
}

template <typename T>
Element TableBody<T>::Render() {
  std::vector<std::vector<Element>> rows_elements;
  std::lock_guard lock(mutex_);
  for (auto it = events_.rbegin(); it != events_.rend(); ++it) {
    const T& event = *it;
    RowType row = BuildRow(event);
    std::vector<Element> cells;
    for (size_t i = 0; i < row.size(); ++i) {
      size_t width = i < col_widths_.size() ? col_widths_[i] : 10;
      cells.push_back(RenderCell(row[i], width, i, event));
    }
    rows_elements.push_back(cells);
  }
  Table body_table(rows_elements);
  return body_table.Render() | bgcolor(Color::Black);
}

ScrollableTable::ScrollableTable(
    std::shared_ptr<TableHeader> header, std::shared_ptr<ITableBody> body,
    rpp::subjects::publish_subject<RedrawSignal>& redraw_subject)
    : header_component_(std::move(header)),
      body_component_(std::move(body)),
      redraw_subject_(redraw_subject) {
  // Create and store persistent renderer and scroller.
  body_renderer_ = Renderer([this] { return body_component_->Render(); });
  body_scroller_ = Scroller(body_renderer_);
  Add(body_scroller_);
}

Element ScrollableTable::Render() {
  recalcColumnWidths();
  return vbox({header_component_->Render(),
               body_scroller_->Render() | size(HEIGHT, EQUAL, 10)});
}

void ScrollableTable::recalcColumnWidths() const {
  const auto header_row = header_component_->getHeaderRow();
  std::vector<std::vector<std::string>> all_rows;
  all_rows.push_back(header_row);
  auto body_rows = body_component_->getRows();
  all_rows.insert(all_rows.end(), body_rows.begin(), body_rows.end());

  std::vector<size_t> new_widths(header_row.size(), 0);
  for (const auto& row : all_rows) {
    for (size_t i = 0; i < row.size(); ++i) {
      new_widths[i] = std::max(new_widths[i], row[i].size());
    }
  }
  for (auto& width : new_widths) width += 2;

  header_component_->setColumnWidths(new_widths);
  body_component_->setColumnWidths(new_widths);
}
}  // namespace component
