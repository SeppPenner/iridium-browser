// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/web_test_support_renderer.h"

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "content/common/renderer.mojom.h"
#include "content/common/unique_name_helper.h"
#include "content/renderer/input/render_widget_input_handler_delegate.h"
#include "content/renderer/loader/request_extra_data.h"
#include "content/renderer/loader/web_worker_fetch_context_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/render_widget.h"
#include "content/shell/renderer/web_test/blink_test_runner.h"
#include "content/shell/renderer/web_test/web_test_render_thread_observer.h"
#include "content/shell/test_runner/test_interfaces.h"
#include "content/shell/test_runner/web_frame_test_proxy.h"
#include "content/shell/test_runner/web_view_test_proxy.h"
#include "content/shell/test_runner/web_widget_test_proxy.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_float_rect.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/icc_profile.h"

namespace content {

namespace {

RenderViewImpl* CreateWebViewTestProxy(CompositorDependencies* compositor_deps,
                                       const mojom::CreateViewParams& params) {
  TestInterfaces* interfaces =
      WebTestRenderThreadObserver::GetInstance()->test_interfaces();

  auto* render_view_proxy = new WebViewTestProxy(compositor_deps, params);
  render_view_proxy->Initialize(interfaces);
  return render_view_proxy;
}

std::unique_ptr<RenderWidget> CreateRenderWidgetForFrame(
    int32_t routing_id,
    CompositorDependencies* compositor_deps,
    bool never_composited,
    mojo::PendingReceiver<mojom::Widget> widget_receiver) {
  return std::make_unique<WebWidgetTestProxy>(routing_id, compositor_deps,
                                              /*hidden=*/true, never_composited,
                                              std::move(widget_receiver));
}

RenderFrameImpl* CreateWebFrameTestProxy(RenderFrameImpl::CreateParams params) {
  // RenderFrameImpl always has a RenderViewImpl for it.
  RenderViewImpl* render_view_impl = params.render_view;

  auto* render_frame_proxy = new WebFrameTestProxy(std::move(params));
  render_frame_proxy->Initialize(render_view_impl);
  return render_frame_proxy;
}

}  // namespace

void EnableWebTestProxyCreation() {
  RenderViewImpl::InstallCreateHook(CreateWebViewTestProxy);
  RenderWidget::InstallCreateForFrameHook(CreateRenderWidgetForFrame);
  RenderFrameImpl::InstallCreateHook(CreateWebFrameTestProxy);
}

void SetWorkerRewriteURLFunction(RewriteURLFunction rewrite_url_function) {
  WebWorkerFetchContextImpl::InstallRewriteURLFunction(rewrite_url_function);
}

void EnableRendererWebTestMode() {
  RenderThreadImpl::current()->enable_web_test_mode();

  UniqueNameHelper::PreserveStableUniqueNameForTesting();
}

int GetLocalSessionHistoryLength(RenderView* render_view) {
  return static_cast<RenderViewImpl*>(render_view)
      ->GetLocalSessionHistoryLengthForTesting();
}

void SetFocusAndActivate(RenderView* render_view, bool enable) {
  static_cast<RenderViewImpl*>(render_view)
      ->SetFocusAndActivateForTesting(enable);
}

void ForceResizeRenderView(RenderView* render_view,
                           const blink::WebSize& new_size) {
  RenderViewImpl* render_view_impl = static_cast<RenderViewImpl*>(render_view);
  RenderFrameImpl* main_frame = render_view_impl->GetMainRenderFrame();
  if (!main_frame)
    return;
  RenderWidget* render_widget = main_frame->GetLocalRootRenderWidget();
  gfx::Rect window_rect(render_widget->WindowRect().x,
                        render_widget->WindowRect().y, new_size.width,
                        new_size.height);
  render_widget->SetWindowRectSynchronouslyForTesting(window_rect);
}

void SetDeviceScaleFactor(RenderView* render_view, float factor) {
  RenderViewImpl* render_view_impl = static_cast<RenderViewImpl*>(render_view);
  RenderFrameImpl* main_frame = render_view_impl->GetMainRenderFrame();
  if (!main_frame)
    return;
  RenderWidget* render_widget = main_frame->GetLocalRootRenderWidget();
  render_widget->SetDeviceScaleFactorForTesting(factor);
}

void SetDeviceColorSpace(RenderView* render_view,
                         const gfx::ColorSpace& color_space) {
  RenderViewImpl* render_view_impl = static_cast<RenderViewImpl*>(render_view);
  RenderFrameImpl* main_frame = render_view_impl->GetMainRenderFrame();
  if (!main_frame)
    return;
  RenderWidget* render_widget = main_frame->GetLocalRootRenderWidget();
  render_widget->SetDeviceColorSpaceForTesting(color_space);
}

void EnableAutoResizeMode(RenderView* render_view,
                          const blink::WebSize& min_size,
                          const blink::WebSize& max_size) {
  RenderViewImpl* render_view_impl = static_cast<RenderViewImpl*>(render_view);
  RenderFrameImpl* main_frame = render_view_impl->GetMainRenderFrame();
  if (!main_frame)
    return;
  RenderWidget* render_widget = main_frame->GetLocalRootRenderWidget();
  render_widget->EnableAutoResizeForTesting(min_size, max_size);
}

void DisableAutoResizeMode(RenderView* render_view,
                           const blink::WebSize& new_size) {
  RenderViewImpl* render_view_impl = static_cast<RenderViewImpl*>(render_view);
  RenderFrameImpl* main_frame = render_view_impl->GetMainRenderFrame();
  if (!main_frame)
    return;
  RenderWidget* render_widget = main_frame->GetLocalRootRenderWidget();
  render_widget->DisableAutoResizeForTesting(new_size);
}

void SchedulerRunIdleTasks(base::OnceClosure callback) {
  blink::scheduler::WebThreadScheduler* scheduler =
      content::RenderThreadImpl::current()->GetWebMainThreadScheduler();
  blink::scheduler::RunIdleTasksForTesting(scheduler, std::move(callback));
}

void ForceTextInputStateUpdateForRenderFrame(RenderFrame* frame) {
  RenderWidget* render_widget =
      static_cast<RenderFrameImpl*>(frame)->GetLocalRootRenderWidget();
  render_widget->ShowVirtualKeyboard();
}

}  // namespace content
