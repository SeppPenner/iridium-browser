// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/interaction_handler_android.h"
#include <algorithm>
#include <vector>
#include "base/android/jni_string.h"
#include "base/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/android/autofill_assistant/generic_ui_controller_android.h"
#include "chrome/browser/android/autofill_assistant/generic_ui_events_android.h"
#include "chrome/browser/android/autofill_assistant/generic_ui_interactions_android.h"
#include "components/autofill_assistant/browser/basic_interactions.h"
#include "components/autofill_assistant/browser/interactions.pb.h"
#include "components/autofill_assistant/browser/ui_delegate.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "components/autofill_assistant/browser/value_util.h"

namespace autofill_assistant {

namespace {

// A simple wrapper around a basic interaction, needed because we can't directly
// bind a repeating callback to a method with non-void return value.
void TryRunConditionalCallback(
    base::WeakPtr<BasicInteractions> basic_interactions,
    const std::string& condition_identifier,
    InteractionHandlerAndroid::InteractionCallback callback) {
  if (!basic_interactions) {
    return;
  }
  basic_interactions->RunConditionalCallback(condition_identifier, callback);
}

base::Optional<EventHandler::EventKey> CreateEventKeyFromProto(
    const EventProto& proto,
    JNIEnv* env,
    std::map<std::string, base::android::ScopedJavaGlobalRef<jobject>>* views,
    base::android::ScopedJavaGlobalRef<jobject> jdelegate) {
  switch (proto.kind_case()) {
    case EventProto::kOnValueChanged:
      return base::Optional<EventHandler::EventKey>(
          {proto.kind_case(), proto.on_value_changed().model_identifier()});
    case EventProto::kOnViewClicked: {
      auto jview = views->find(proto.on_view_clicked().view_identifier());
      if (jview == views->end()) {
        VLOG(1) << "Invalid OnViewClickedEventProto: no view with id='"
                << proto.on_view_clicked().view_identifier() << "' found";
        return base::nullopt;
      }
      android_events::SetOnClickListener(env, jview->second, jdelegate,
                                         proto.on_view_clicked());
      return base::Optional<EventHandler::EventKey>(
          {proto.kind_case(), proto.on_view_clicked().view_identifier()});
    }
    case EventProto::kOnUserActionCalled:
      return base::Optional<EventHandler::EventKey>(
          {proto.kind_case(),
           proto.on_user_action_called().user_action_identifier()});
    case EventProto::kOnTextLinkClicked:
      if (!proto.on_text_link_clicked().has_text_link()) {
        VLOG(1) << "Invalid OnTextLinkClickedProto: no text_link specified";
        return base::nullopt;
      }
      return base::Optional<EventHandler::EventKey>(
          {proto.kind_case(),
           base::NumberToString(proto.on_text_link_clicked().text_link())});
    case EventProto::KIND_NOT_SET:
      VLOG(1) << "Error creating event: kind not set";
      return base::nullopt;
  }
}

base::Optional<InteractionHandlerAndroid::InteractionCallback>
CreateInteractionCallbackFromProto(
    const CallbackProto& proto,
    UserModel* user_model,
    BasicInteractions* basic_interactions,
    std::map<std::string, base::android::ScopedJavaGlobalRef<jobject>>* views,
    base::android::ScopedJavaGlobalRef<jobject> jcontext,
    base::android::ScopedJavaGlobalRef<jobject> jdelegate) {
  switch (proto.kind_case()) {
    case CallbackProto::kSetValue:
      if (proto.set_value().model_identifier().empty()) {
        VLOG(1)
            << "Error creating SetValue interaction: model_identifier not set";
        return base::nullopt;
      }
      return base::Optional<InteractionHandlerAndroid::InteractionCallback>(
          base::BindRepeating(&android_interactions::SetValue,
                              basic_interactions->GetWeakPtr(),
                              proto.set_value()));
    case CallbackProto::kShowInfoPopup: {
      return base::Optional<InteractionHandlerAndroid::InteractionCallback>(
          base::BindRepeating(&android_interactions::ShowInfoPopup,
                              proto.show_info_popup().info_popup(), jcontext));
    }
    case CallbackProto::kShowListPopup:
      if (proto.show_list_popup().item_names_model_identifier().empty()) {
        VLOG(1) << "Error creating ShowListPopup interaction: "
                   "items_list_model_identifier not set";
        return base::nullopt;
      }
      if (proto.show_list_popup()
              .selected_item_indices_model_identifier()
              .empty()) {
        VLOG(1) << "Error creating ShowListPopup interaction: "
                   "selected_item_indices_model_identifier not set";
        return base::nullopt;
      }
      return base::Optional<InteractionHandlerAndroid::InteractionCallback>(
          base::BindRepeating(&android_interactions::ShowListPopup,
                              user_model->GetWeakPtr(), proto.show_list_popup(),
                              jcontext, jdelegate));
    case CallbackProto::kComputeValue:
      if (proto.compute_value().result_model_identifier().empty()) {
        VLOG(1) << "Error creating ComputeValue interaction: "
                   "result_model_identifier empty";
        return base::nullopt;
      }
      return base::Optional<InteractionHandlerAndroid::InteractionCallback>(
          base::BindRepeating(&android_interactions::ComputeValue,
                              basic_interactions->GetWeakPtr(),
                              proto.compute_value()));
    case CallbackProto::kSetUserActions:
      if (proto.set_user_actions().model_identifier().empty()) {
        VLOG(1) << "Error creating SetUserActions interaction: "
                   "model_identifier not set";
        return base::nullopt;
      }
      return base::Optional<InteractionHandlerAndroid::InteractionCallback>(
          base::BindRepeating(&android_interactions::SetUserActions,
                              basic_interactions->GetWeakPtr(),
                              proto.set_user_actions()));
    case CallbackProto::kEndAction:
      return base::Optional<InteractionHandlerAndroid::InteractionCallback>(
          base::BindRepeating(&android_interactions::EndAction,
                              basic_interactions->GetWeakPtr(),
                              proto.end_action()));
    case CallbackProto::kShowCalendarPopup:
      if (proto.show_calendar_popup().date_model_identifier().empty()) {
        VLOG(1) << "Error creating ShowCalendarPopup interaction: "
                   "date_model_identifier not set";
        return base::nullopt;
      }
      return base::Optional<InteractionHandlerAndroid::InteractionCallback>(
          base::BindRepeating(&android_interactions::ShowCalendarPopup,
                              user_model->GetWeakPtr(),
                              proto.show_calendar_popup(), jcontext,
                              jdelegate));
    case CallbackProto::kSetText:
      if (proto.set_text().model_identifier().empty()) {
        VLOG(1) << "Error creating SetText interaction: "
                   "model_identifier not set";
        return base::nullopt;
      }
      if (proto.set_text().view_identifier().empty()) {
        VLOG(1) << "Error creating SetText interaction: "
                   "view_identifier not set";
        return base::nullopt;
      }
      return base::Optional<InteractionHandlerAndroid::InteractionCallback>(
          base::BindRepeating(&android_interactions::SetViewText,
                              user_model->GetWeakPtr(), proto.set_text(), views,
                              jdelegate));
    case CallbackProto::kToggleUserAction:
      if (proto.toggle_user_action().user_actions_model_identifier().empty()) {
        VLOG(1) << "Error creating ToggleUserAction interaction: "
                   "user_actions_model_identifier not set";
        return base::nullopt;
      }
      if (proto.toggle_user_action().user_action_identifier().empty()) {
        VLOG(1) << "Error creating ToggleUserAction interaction: "
                   "user_action_identifier not set";
        return base::nullopt;
      }
      if (proto.toggle_user_action().enabled_model_identifier().empty()) {
        VLOG(1) << "Error creating ToggleUserAction interaction: "
                   "enabled_model_identifier not set";
        return base::nullopt;
      }
      return base::Optional<InteractionHandlerAndroid::InteractionCallback>(
          base::BindRepeating(&android_interactions::ToggleUserAction,
                              basic_interactions->GetWeakPtr(),
                              proto.toggle_user_action()));
    case CallbackProto::kSetViewVisibility:
      if (proto.set_view_visibility().view_identifier().empty()) {
        VLOG(1) << "Error creating SetViewVisibility interaction: "
                   "view_identifier not set";
        return base::nullopt;
      }
      if (proto.set_view_visibility().model_identifier().empty()) {
        VLOG(1) << "Error creating SetViewVisibility interaction: "
                   "model_identifier not set";
        return base::nullopt;
      }
      return base::Optional<InteractionHandlerAndroid::InteractionCallback>(
          base::BindRepeating(&android_interactions::SetViewVisibility,
                              user_model->GetWeakPtr(),
                              proto.set_view_visibility(), views));
    case CallbackProto::KIND_NOT_SET:
      VLOG(1) << "Error creating interaction: kind not set";
      return base::nullopt;
  }
}

}  // namespace

InteractionHandlerAndroid::InteractionHandlerAndroid(
    EventHandler* event_handler,
    UserModel* user_model,
    BasicInteractions* basic_interactions,
    std::map<std::string, base::android::ScopedJavaGlobalRef<jobject>>* views,
    base::android::ScopedJavaGlobalRef<jobject> jcontext,
    base::android::ScopedJavaGlobalRef<jobject> jdelegate)
    : event_handler_(event_handler),
      user_model_(user_model),
      basic_interactions_(basic_interactions),
      views_(views),
      jcontext_(jcontext),
      jdelegate_(jdelegate) {}

InteractionHandlerAndroid::~InteractionHandlerAndroid() {
  event_handler_->RemoveObserver(this);
}

void InteractionHandlerAndroid::StartListening() {
  is_listening_ = true;
  event_handler_->AddObserver(this);
}

void InteractionHandlerAndroid::StopListening() {
  event_handler_->RemoveObserver(this);
  is_listening_ = false;
}

bool InteractionHandlerAndroid::AddInteractionsFromProto(
    const InteractionsProto& proto) {
  if (is_listening_) {
    NOTREACHED() << "Interactions can not be added while listening to events!";
    return false;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  for (const auto& interaction_proto : proto.interactions()) {
    auto key = CreateEventKeyFromProto(interaction_proto.trigger_event(), env,
                                       views_, jdelegate_);
    if (!key) {
      VLOG(1) << "Invalid trigger event for interaction";
      return false;
    }

    for (const auto& callback_proto : interaction_proto.callbacks()) {
      auto callback = CreateInteractionCallbackFromProto(
          callback_proto, user_model_, basic_interactions_, views_, jcontext_,
          jdelegate_);
      if (!callback) {
        VLOG(1) << "Invalid callback for interaction";
        return false;
      }
      // Wrap callback in condition handler if necessary.
      if (callback_proto.has_condition_model_identifier()) {
        callback =
            base::Optional<InteractionHandlerAndroid::InteractionCallback>(
                base::BindRepeating(&TryRunConditionalCallback,
                                    basic_interactions_->GetWeakPtr(),
                                    callback_proto.condition_model_identifier(),
                                    *callback));
      }
      AddInteraction(*key, *callback);
    }
  }
  return true;
}

void InteractionHandlerAndroid::AddInteraction(
    const EventHandler::EventKey& key,
    const InteractionCallback& callback) {
  interactions_[key].emplace_back(callback);
}

void InteractionHandlerAndroid::OnEvent(const EventHandler::EventKey& key) {
  auto it = interactions_.find(key);
  if (it != interactions_.end()) {
    for (auto& callback : it->second) {
      callback.Run();
    }
  }
}

}  // namespace autofill_assistant
