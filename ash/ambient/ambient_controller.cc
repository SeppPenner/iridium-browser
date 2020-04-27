// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_controller.h"

#include <string>

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/model/ambient_backend_model_observer.h"
#include "ash/ambient/ui/ambient_container_view.h"
#include "ash/ambient/util/ambient_util.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/public/cpp/ambient/ambient_mode_state.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/public/cpp/ambient/photo_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "components/prefs/pref_registry_simple.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

bool CanStartAmbientMode() {
  return chromeos::features::IsAmbientModeEnabled() && PhotoController::Get() &&
         !ambient::util::IsShowing(LockScreen::ScreenType::kLogin);
}

void CloseAssistantUi() {
  DCHECK(AssistantUiController::Get());
  AssistantUiController::Get()->CloseUi(
      chromeos::assistant::mojom::AssistantExitPoint::kUnspecified);
}

}  // namespace

// static
void AmbientController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  if (chromeos::features::IsAmbientModeEnabled()) {
    registry->RegisterStringPref(ash::ambient::prefs::kAmbientBackdropClientId,
                                 std::string());

    // Do not sync across devices to allow different usages for different
    // devices.
    registry->RegisterBooleanPref(ash::ambient::prefs::kAmbientModeEnabled,
                                  true);
  }
}

AmbientController::AmbientController() {
  ambient_state_.AddObserver(this);
  // |SessionController| is initialized before |this| in Shell.
  Shell::Get()->session_controller()->AddObserver(this);
}

AmbientController::~AmbientController() {
  // |SessionController| is destroyed after |this| in Shell.
  Shell::Get()->session_controller()->RemoveObserver(this);
  ambient_state_.RemoveObserver(this);

  if (container_view_)
    DestroyContainerView();
}

void AmbientController::OnWidgetDestroying(views::Widget* widget) {
  refresh_timer_.Stop();
  ambient_backend_model_.Clear();
  weak_factory_.InvalidateWeakPtrs();
  container_view_->GetWidget()->RemoveObserver(this);
  container_view_ = nullptr;

  // Call CloseUi() explicitly to sync states to |AssistantUiController|.
  // This is a no-op if the UI has already been closed before the widget gets
  // destroyed.
  CloseAssistantUi();
}

void AmbientController::OnAmbientModeEnabled(bool enabled) {
  if (enabled) {
    CreateContainerView();
    container_view_->GetWidget()->Show();
    RefreshImage();
  } else {
    DestroyContainerView();
  }
}

void AmbientController::OnLockStateChanged(bool locked) {
  if (locked) {
    // Show ambient mode when entering lock screen.
    DCHECK(!container_view_);
    Show();
  } else {
    // Destroy ambient mode after user re-login.
    Destroy();
  }
}

void AmbientController::Show() {
  if (!CanStartAmbientMode()) {
    // TODO(wutao): Show a toast to indicate that Ambient mode is not ready.
    return;
  }

  // CloseUi to ensure the embedded Assistant UI doesn't exist when entering
  // Ambient mode to avoid strange behavior caused by the embedded UI was
  // only hidden at that time. This will be a no-op if UI was already closed.
  // TODO(meilinw): Handle embedded UI.
  CloseAssistantUi();

  ambient_state_.SetAmbientModeEnabled(true);
}

void AmbientController::Destroy() {
  ambient_state_.SetAmbientModeEnabled(false);
}

void AmbientController::Toggle() {
  if (container_view_)
    Destroy();
  else
    Show();
}

void AmbientController::OnBackgroundPhotoEvents() {
  refresh_timer_.Stop();

  // Move the |AmbientModeContainer| beneath the |LockScreenWidget| to show the
  // lock screen contents on top before the fade-out animation.
  auto* ambient_window = container_view_->GetWidget()->GetNativeWindow();
  ambient_window->parent()->StackChildAtBottom(ambient_window);

  // Start fading out the current background photo.
  StartFadeOutAnimation();
}

void AmbientController::StartFadeOutAnimation() {
  // We fade out the |PhotoView| on its own layer instead of using the general
  // layer of the widget, otherwise it will reveal the color of the lockscreen
  // wallpaper beneath.
  container_view_->FadeOutPhotoView();
}

void AmbientController::CreateContainerView() {
  DCHECK(!container_view_);
  container_view_ = new AmbientContainerView(&delegate_);
  container_view_->GetWidget()->AddObserver(this);
}

void AmbientController::DestroyContainerView() {
  // |container_view_|'s widget is owned by its native widget. After calling
  // |CloseNow|, |OnWidgetDestroying| will be triggered immediately to reset
  // |container_view_| to nullptr.
  if (container_view_)
    container_view_->GetWidget()->CloseNow();
}

void AmbientController::RefreshImage() {
  if (ambient_backend_model_.ShouldFetchImmediately()) {
    // TODO(b/140032139): Defer downloading image if it is animating.
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AmbientController::GetNextImage,
                       weak_factory_.GetWeakPtr()),
        kAnimationDuration);
  } else {
    ambient_backend_model_.ShowNextImage();
    ScheduleRefreshImage();
  }
}

void AmbientController::ScheduleRefreshImage() {
  base::TimeDelta refresh_interval;
  if (!ambient_backend_model_.ShouldFetchImmediately()) {
    // TODO(b/139953713): Change to a correct time interval.
    refresh_interval = base::TimeDelta::FromSeconds(5);
  }

  refresh_timer_.Start(FROM_HERE, refresh_interval,
                       base::BindOnce(&AmbientController::RefreshImage,
                                      weak_factory_.GetWeakPtr()));
}

void AmbientController::GetNextImage() {
  // Start requesting photo and weather information from the backdrop server.
  PhotoController::Get()->GetNextScreenUpdateInfo(
      base::BindOnce(&AmbientController::OnPhotoDownloaded,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&AmbientController::OnWeatherConditionIconDownloaded,
                     weak_factory_.GetWeakPtr()));
}

void AmbientController::OnPhotoDownloaded(const gfx::ImageSkia& image) {
  // TODO(b/148485116): Implement retry logic.
  if (image.isNull())
    return;

  DCHECK(!image.isNull());
  ambient_backend_model_.AddNextImage(image);
  ScheduleRefreshImage();
}

void AmbientController::OnWeatherConditionIconDownloaded(
    base::Optional<float> temp_f,
    const gfx::ImageSkia& icon) {
  // For now we only show the weather card when both fields have values.
  // TODO(meilinw): optimize the behavior with more specific error handling.
  if (icon.isNull() || !temp_f.has_value())
    return;

  ambient_backend_model_.UpdateWeatherInfo(icon, temp_f.value());
}

}  // namespace ash
