// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/chrome_native_app_window_views.h"

#include <stddef.h>
#include <utility>

#include "apps/ui/views/app_window_frame_view.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/extensions/chrome_app_icon.h"
#include "chrome/browser/extensions/chrome_app_icon_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/accelerator_table.h"
#include "chrome/browser/ui/views/extensions/extension_keybinding_registry_views.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/zoom/page_zoom.h"
#include "components/zoom/zoom_controller.h"
#include "extensions/browser/app_window/app_delegate.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

using extensions::AppWindow;

namespace {

const AcceleratorMapping kAppWindowAcceleratorMap[] = {
  { ui::VKEY_W, ui::EF_CONTROL_DOWN, IDC_CLOSE_WINDOW },
  { ui::VKEY_W, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, IDC_CLOSE_WINDOW },
  { ui::VKEY_F4, ui::EF_ALT_DOWN, IDC_CLOSE_WINDOW },
};

// These accelerators will only be available in kiosk mode. These allow the
// user to manually zoom app windows. This is only necessary in kiosk mode
// (in normal mode, the user can zoom via the screen magnifier).
// TODO(xiyuan): Write a test for kiosk accelerators.
const AcceleratorMapping kAppWindowKioskAppModeAcceleratorMap[] = {
  { ui::VKEY_OEM_MINUS, ui::EF_CONTROL_DOWN, IDC_ZOOM_MINUS },
  { ui::VKEY_OEM_MINUS, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
    IDC_ZOOM_MINUS },
  { ui::VKEY_SUBTRACT, ui::EF_CONTROL_DOWN, IDC_ZOOM_MINUS },
  { ui::VKEY_OEM_PLUS, ui::EF_CONTROL_DOWN, IDC_ZOOM_PLUS },
  { ui::VKEY_OEM_PLUS, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, IDC_ZOOM_PLUS },
  { ui::VKEY_ADD, ui::EF_CONTROL_DOWN, IDC_ZOOM_PLUS },
  { ui::VKEY_0, ui::EF_CONTROL_DOWN, IDC_ZOOM_NORMAL },
  { ui::VKEY_NUMPAD0, ui::EF_CONTROL_DOWN, IDC_ZOOM_NORMAL },
};

std::map<ui::Accelerator, int> AcceleratorsFromMapping(
    const AcceleratorMapping mapping_array[],
    size_t mapping_length) {
  std::map<ui::Accelerator, int> mapping;
  for (size_t i = 0; i < mapping_length; ++i) {
    ui::Accelerator accelerator(mapping_array[i].keycode,
                                mapping_array[i].modifiers);
    mapping.insert(std::make_pair(accelerator, mapping_array[i].command_id));
  }

  return mapping;
}

const std::map<ui::Accelerator, int>& GetAcceleratorTable() {
  if (!chrome::IsRunningInForcedAppMode()) {
    static base::NoDestructor<std::map<ui::Accelerator, int>> accelerators(
        AcceleratorsFromMapping(kAppWindowAcceleratorMap,
                                base::size(kAppWindowAcceleratorMap)));
    return *accelerators;
  }

  static base::NoDestructor<std::map<ui::Accelerator, int>>
      app_mode_accelerators([]() {
        std::map<ui::Accelerator, int> mapping = AcceleratorsFromMapping(
            kAppWindowAcceleratorMap, base::size(kAppWindowAcceleratorMap));
        std::map<ui::Accelerator, int> kiosk_mapping = AcceleratorsFromMapping(
            kAppWindowKioskAppModeAcceleratorMap,
            base::size(kAppWindowKioskAppModeAcceleratorMap));
        mapping.insert(std::begin(kiosk_mapping), std::end(kiosk_mapping));
        return mapping;
      }());
  return *app_mode_accelerators;
}

}  // namespace

ChromeNativeAppWindowViews::ChromeNativeAppWindowViews()
    : has_frame_color_(false),
      active_frame_color_(SK_ColorBLACK),
      inactive_frame_color_(SK_ColorBLACK) {
}

ChromeNativeAppWindowViews::~ChromeNativeAppWindowViews() {}

void ChromeNativeAppWindowViews::OnBeforeWidgetInit(
    const AppWindow::CreateParams& create_params,
    views::Widget::InitParams* init_params,
    views::Widget* widget) {
}

void ChromeNativeAppWindowViews::InitializeDefaultWindow(
    const AppWindow::CreateParams& create_params) {
  views::Widget::InitParams init_params(views::Widget::InitParams::TYPE_WINDOW);
  init_params.delegate = this;
  init_params.remove_standard_frame = ShouldRemoveStandardFrame();
  init_params.use_system_default_icon = true;
  if (create_params.alpha_enabled) {
    init_params.opacity =
        views::Widget::InitParams::WindowOpacity::kTranslucent;

    // The given window is most likely not rectangular since it uses
    // transparency and has no standard frame, don't show a shadow for it.
    // TODO(skuhne): If we run into an application which should have a shadow
    // but does not have, a new attribute has to be added.
    if (IsFrameless())
      init_params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  }
  if (create_params.always_on_top)
    init_params.z_order = ui::ZOrderLevel::kFloatingWindow;
  init_params.visible_on_all_workspaces =
      create_params.visible_on_all_workspaces;

  OnBeforeWidgetInit(create_params, &init_params, widget());
  widget()->Init(std::move(init_params));

  // The frame insets are required to resolve the bounds specifications
  // correctly. So we set the window bounds and constraints now.
  gfx::Insets frame_insets = GetFrameInsets();
  gfx::Rect window_bounds = create_params.GetInitialWindowBounds(frame_insets);
  SetContentSizeConstraints(create_params.GetContentMinimumSize(frame_insets),
                            create_params.GetContentMaximumSize(frame_insets));
  if (!window_bounds.IsEmpty()) {
    auto position_specified = [](const gfx::Rect& window_bounds) -> bool {
      using BoundsSpecification = AppWindow::BoundsSpecification;
      return window_bounds.x() != BoundsSpecification::kUnspecifiedPosition &&
             window_bounds.y() != BoundsSpecification::kUnspecifiedPosition;
    };

    // Windows without saved bounds should be centered.
    const bool center_window = !position_specified(window_bounds);

    // Adjust bounds to be on the display for new windows.
    AdjustBoundsToBeVisibleOnDisplayForNewWindows(&window_bounds);

    // Widget::SetBounds should not take unspecified coordinates.
    if (position_specified(window_bounds))
      widget()->SetBounds(window_bounds);

    if (center_window)
      widget()->CenterWindow(window_bounds.size());
  }

#if defined(OS_CHROMEOS)
  if (create_params.is_ime_window)
    return;
#endif

  // Register accelarators supported by app windows.
  views::FocusManager* focus_manager = GetFocusManager();
  const std::map<ui::Accelerator, int>& accelerator_table =
      GetAcceleratorTable();
  const bool is_kiosk_app_mode = chrome::IsRunningInForcedAppMode();

  // Ensures that kiosk mode accelerators are enabled when in kiosk mode (to be
  // future proof). This is needed because GetAcceleratorTable() uses a static
  // to store data and only checks kiosk mode once. If a platform app is
  // launched before kiosk mode starts, the kiosk accelerators will not be
  // registered. This CHECK catches the case.
  CHECK(!is_kiosk_app_mode ||
        accelerator_table.size() ==
            base::size(kAppWindowAcceleratorMap) +
                base::size(kAppWindowKioskAppModeAcceleratorMap));

  // Ensure there is a ZoomController in kiosk mode, otherwise the processing
  // of the accelerators will cause a crash. Note CHECK here because DCHECK
  // will not be noticed, as this could only be relevant on real hardware.
  CHECK(!is_kiosk_app_mode ||
        zoom::ZoomController::FromWebContents(web_view()->GetWebContents()));

  for (auto iter = accelerator_table.begin(); iter != accelerator_table.end();
       ++iter) {
    if (is_kiosk_app_mode &&
        !chrome::IsCommandAllowedInAppMode(iter->second, /* is_popup */ false))
      continue;

    focus_manager->RegisterAccelerator(
        iter->first, ui::AcceleratorManager::kNormalPriority, this);
  }
}

views::NonClientFrameView*
ChromeNativeAppWindowViews::CreateStandardDesktopAppFrame() {
  return views::WidgetDelegateView::CreateNonClientFrameView(widget());
}

bool ChromeNativeAppWindowViews::ShouldRemoveStandardFrame() {
  return IsFrameless() || has_frame_color_;
}

void ChromeNativeAppWindowViews::AdjustBoundsToBeVisibleOnDisplayForNewWindows(
    gfx::Rect* out_bounds) {}

// ui::BaseWindow implementation.

gfx::Rect ChromeNativeAppWindowViews::GetRestoredBounds() const {
  return widget()->GetRestoredBounds();
}

ui::WindowShowState ChromeNativeAppWindowViews::GetRestoredState() const {
  if (IsMaximized())
    return ui::SHOW_STATE_MAXIMIZED;
  if (IsFullscreen())
    return ui::SHOW_STATE_FULLSCREEN;

  return ui::SHOW_STATE_NORMAL;
}

ui::ZOrderLevel ChromeNativeAppWindowViews::GetZOrderLevel() const {
  return widget()->GetZOrderLevel();
}

// views::WidgetDelegate implementation.

gfx::ImageSkia ChromeNativeAppWindowViews::GetWindowAppIcon() {
  // Resulting icon is cached in aura::client::kAppIconKey window property.
  const gfx::Image& custom_image = app_window()->custom_app_icon();
  if (app_window()->app_icon_url().is_valid() &&
      app_window()->show_in_shelf()) {
    EnsureAppIconCreated();
    gfx::Image base_image =
        !custom_image.IsEmpty()
            ? custom_image
            : gfx::Image(extensions::util::GetDefaultAppIcon());
    // Scale the icon to EXTENSION_ICON_LARGE.
    const int large_icon_size = extension_misc::EXTENSION_ICON_LARGE;
    if (base_image.Width() != large_icon_size ||
        base_image.Height() != large_icon_size) {
      gfx::ImageSkia resized_image =
          gfx::ImageSkiaOperations::CreateResizedImage(
              base_image.AsImageSkia(), skia::ImageOperations::RESIZE_BEST,
              gfx::Size(large_icon_size, large_icon_size));
      return gfx::ImageSkiaOperations::CreateIconWithBadge(
          resized_image, app_icon_->image_skia());
    }
    return gfx::ImageSkiaOperations::CreateIconWithBadge(
        base_image.AsImageSkia(), app_icon_->image_skia());
  }

  if (!custom_image.IsEmpty())
    return *custom_image.ToImageSkia();
  EnsureAppIconCreated();
  return app_icon_->image_skia();
}

gfx::ImageSkia ChromeNativeAppWindowViews::GetWindowIcon() {
  // Resulting icon is cached in aura::client::kWindowIconKey window property.
  content::WebContents* web_contents = app_window()->web_contents();
  if (web_contents) {
    favicon::FaviconDriver* favicon_driver =
        favicon::ContentFaviconDriver::FromWebContents(web_contents);
    gfx::Image app_icon = favicon_driver->GetFavicon();
    if (!app_icon.IsEmpty())
      return *app_icon.ToImageSkia();
  }
  return gfx::ImageSkia();
}

views::NonClientFrameView* ChromeNativeAppWindowViews::CreateNonClientFrameView(
    views::Widget* widget) {
  return (IsFrameless() || has_frame_color_) ?
      CreateNonStandardAppFrame() : CreateStandardDesktopAppFrame();
}

bool ChromeNativeAppWindowViews::WidgetHasHitTestMask() const {
  return shape_ != NULL;
}

void ChromeNativeAppWindowViews::GetWidgetHitTestMask(SkPath* mask) const {
  shape_->getBoundaryPath(mask);
}

// views::View implementation.

bool ChromeNativeAppWindowViews::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  const std::map<ui::Accelerator, int>& accelerator_table =
      GetAcceleratorTable();
  auto iter = accelerator_table.find(accelerator);
  DCHECK(iter != accelerator_table.end());
  int command_id = iter->second;
  switch (command_id) {
    case IDC_CLOSE_WINDOW:
      Close();
      return true;
    case IDC_ZOOM_MINUS:
      zoom::PageZoom::Zoom(web_view()->GetWebContents(),
                           content::PAGE_ZOOM_OUT);
      return true;
    case IDC_ZOOM_NORMAL:
      zoom::PageZoom::Zoom(web_view()->GetWebContents(),
                           content::PAGE_ZOOM_RESET);
      return true;
    case IDC_ZOOM_PLUS:
      zoom::PageZoom::Zoom(web_view()->GetWebContents(), content::PAGE_ZOOM_IN);
      return true;
    default:
      NOTREACHED() << "Unknown accelerator sent to app window.";
  }
  return NativeAppWindowViews::AcceleratorPressed(accelerator);
}

// NativeAppWindow implementation.

void ChromeNativeAppWindowViews::SetFullscreen(int fullscreen_types) {
  widget()->SetFullscreen(fullscreen_types != AppWindow::FULLSCREEN_TYPE_NONE);
}

bool ChromeNativeAppWindowViews::IsFullscreenOrPending() const {
  return widget()->IsFullscreen();
}

void ChromeNativeAppWindowViews::UpdateShape(
    std::unique_ptr<ShapeRects> rects) {
  shape_rects_ = std::move(rects);

  // Build a region from the list of rects when it is supplied.
  std::unique_ptr<SkRegion> region;
  if (shape_rects_) {
    region = std::make_unique<SkRegion>();
    for (const gfx::Rect& input_rect : *shape_rects_.get())
      region->op(gfx::RectToSkIRect(input_rect), SkRegion::kUnion_Op);
  }
  shape_ = std::move(region);
  widget()->SetShape(shape() ? std::make_unique<ShapeRects>(*shape_rects_)
                             : nullptr);
  widget()->OnSizeConstraintsChanged();
}

bool ChromeNativeAppWindowViews::HasFrameColor() const {
  return has_frame_color_;
}

SkColor ChromeNativeAppWindowViews::ActiveFrameColor() const {
  return active_frame_color_;
}

SkColor ChromeNativeAppWindowViews::InactiveFrameColor() const {
  return inactive_frame_color_;
}

// NativeAppWindowViews implementation.

void ChromeNativeAppWindowViews::InitializeWindow(
    AppWindow* app_window,
    const AppWindow::CreateParams& create_params) {
  DCHECK(widget());
  has_frame_color_ = create_params.has_frame_color;
  active_frame_color_ = create_params.active_frame_color;
  inactive_frame_color_ = create_params.inactive_frame_color;
  InitializeDefaultWindow(create_params);
  extension_keybinding_registry_ =
      std::make_unique<ExtensionKeybindingRegistryViews>(
          Profile::FromBrowserContext(app_window->browser_context()),
          widget()->GetFocusManager(),
          extensions::ExtensionKeybindingRegistry::PLATFORM_APPS_ONLY, nullptr);
}

void ChromeNativeAppWindowViews::EnsureAppIconCreated() {
  if (app_icon_ && app_icon_->IsValid())
    return;

  // To avoid recursive call, reset the smart pointer. It will be checked in
  // OnIconUpdated to determine if this is a real update or the initial callback
  // on icon creation.
  app_icon_.reset();
  app_icon_ =
      extensions::ChromeAppIconService::Get(app_window()->browser_context())
          ->CreateIcon(this, app_window()->extension_id(),
                       app_window()->app_delegate()->PreferredIconSize());
}

void ChromeNativeAppWindowViews::OnIconUpdated(
    extensions::ChromeAppIcon* icon) {
  if (!app_icon_)
    return;
  DCHECK_EQ(app_icon_.get(), icon);
  UpdateWindowIcon();
}
