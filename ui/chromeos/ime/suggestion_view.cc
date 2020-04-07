// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/ime/suggestion_view.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace ui {
namespace ime {

namespace {

// Creates the suggestion label, and returns it (never returns nullptr).
// The label text is not set in this function.
std::unique_ptr<views::Label> CreateSuggestionLabel() {
  std::unique_ptr<views::Label> suggestion_label =
      std::make_unique<views::Label>();

  suggestion_label->SetFontList(kSuggestionFont);
  suggestion_label->SetEnabledColor(kSuggestionLabelColor);
  suggestion_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  suggestion_label->SetBorder(
      views::CreateEmptyBorder(gfx::Insets(kPadding / 2, 0)));

  return suggestion_label;
}

// Creates the "tab" annotation label, and return it (never returns nullptr).
std::unique_ptr<views::Label> CreateAnnotationLabel() {
  std::unique_ptr<views::Label> annotation_label =
      std::make_unique<views::Label>();
  annotation_label->SetFontList(kAnnotationFont);
  annotation_label->SetEnabledColor(kSuggestionLabelColor);
  annotation_label->SetHorizontalAlignment(gfx::ALIGN_CENTER);

  // Set insets.
  const gfx::Insets insets(0, 0, 0, kPadding / 2);
  annotation_label->SetBorder(views::CreateRoundedRectBorder(
      kAnnotationBorderThickness, kAnnotationCornerRadius, insets,
      kSuggestionLabelColor));

  // Set text.
  annotation_label->SetText(base::UTF8ToUTF16(kTabKey));

  return annotation_label;
}

}  // namespace

SuggestionView::SuggestionView() {
  suggestion_label_ = AddChildView(CreateSuggestionLabel());
  annotation_label_ = AddChildView(CreateAnnotationLabel());
}

SuggestionView::~SuggestionView() = default;

void SuggestionView::SetView(const base::string16& text, const bool show_tab) {
  suggestion_label_->SetText(text);
  suggestion_width_ = suggestion_label_->GetPreferredSize().width();
  annotation_label_->SetVisible(show_tab);
}

const char* SuggestionView::GetClassName() const {
  return "SuggestionView";
}

void SuggestionView::Layout() {
  suggestion_label_->SetBounds(kPadding, 0, suggestion_width_, height());

  if (annotation_label_->GetVisible()) {
    int annotation_left = kPadding + suggestion_width_ + kPadding;
    int right = bounds().right();
    annotation_label_->SetBounds(annotation_left, kAnnotationPaddingHeight,
                                 right - annotation_left - kPadding / 2,
                                 height() - 2 * kAnnotationPaddingHeight);
  }
}

gfx::Size SuggestionView::CalculatePreferredSize() const {
  gfx::Size size;

  gfx::Size suggestion_size = suggestion_label_->GetPreferredSize();
  suggestion_size.SetToMax(gfx::Size(suggestion_width_, 0));
  size.Enlarge(suggestion_size.width() + 2 * kPadding, 0);
  size.SetToMax(suggestion_size);
  if (annotation_label_->GetVisible()) {
    gfx::Size annotation_size = annotation_label_->GetPreferredSize();
    size.Enlarge(annotation_size.width() + kPadding, 0);
  }
  return size;
}

}  // namespace ime
}  // namespace ui
