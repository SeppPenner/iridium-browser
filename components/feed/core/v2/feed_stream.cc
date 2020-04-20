// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/feed_stream.h"

#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/ui.pb.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/metrics_reporter.h"
#include "components/feed/core/v2/refresh_task_scheduler.h"
#include "components/feed/core/v2/scheduling.h"
#include "components/feed/core/v2/stream_model.h"
#include "components/feed/core/v2/stream_model_update_request.h"
#include "components/feed/core/v2/tasks/load_stream_task.h"
#include "components/feed/core/v2/tasks/wait_for_store_initialize_task.h"
#include "components/prefs/pref_service.h"

namespace feed {
namespace {
constexpr char kZeroStateSliceId[] = "zero-state";
}

// Tracks UI changes in |StreamModel| and forwards them to |SurfaceInterface|s.
class FeedStream::SurfaceUpdater : public StreamModel::Observer {
 public:
  using ContentRevision = ContentRevision;
  explicit SurfaceUpdater(const base::ObserverList<SurfaceInterface>* surfaces)
      : surfaces_(surfaces) {}
  ~SurfaceUpdater() override = default;
  SurfaceUpdater(const SurfaceUpdater&) = delete;
  SurfaceUpdater& operator=(const SurfaceUpdater&) = delete;

  void SetModel(StreamModel* model) {
    if (model_ == model)
      return;
    if (model_)
      model_->SetObserver(nullptr);
    model_ = model;
    if (model_) {
      model_->SetObserver(this);

      const std::vector<ContentRevision>& content_list =
          model_->GetContentList();
      current_content_set_.insert(content_list.begin(), content_list.end());
      for (SurfaceInterface& surface : *surfaces_) {
        surface.StreamUpdate(GetUpdateForNewSurface(*model_));
      }
    }
  }

  // StreamModel::Observer.
  void OnUiUpdate(const StreamModel::UiUpdate& update) override {
    DCHECK(model_);  // The update comes from the model.

    if (!update.content_list_changed)
      return;
    feedui::StreamUpdate stream_update;
    const std::vector<ContentRevision>& content_list = model_->GetContentList();
    for (ContentRevision content_revision : content_list) {
      AddSliceUpdate(*model_, content_revision,
                     current_content_set_.count(content_revision) == 0,
                     &stream_update);
    }
    for (const StreamModel::UiUpdate::SharedStateInfo& info :
         update.shared_states) {
      if (info.updated)
        AddSharedState(*model_, info.shared_state_id, &stream_update);
    }

    current_content_set_.clear();
    current_content_set_.insert(content_list.begin(), content_list.end());

    for (SurfaceInterface& surface : *surfaces_) {
      surface.StreamUpdate(stream_update);
    }
  }

  // Sends the initial stream state to a newly connected surface.
  void SurfaceAdded(SurfaceInterface* surface) {
    if (model_) {
      surface->StreamUpdate(GetUpdateForNewSurface(*model_));
    }
  }

  void LoadStreamStarted() {
    if (model_)
      return;
    for (SurfaceInterface& surface : *surfaces_) {
      SendLoadingSpinnerUpdate(&surface);
    }
  }

  void LoadStreamFailed(LoadStreamStatus load_stream_status) {
    auto zero_state_type = feedui::ZeroStateSlice::NO_CARDS_AVAILABLE;
    switch (load_stream_status) {
      case LoadStreamStatus::kProtoTranslationFailed:
      case LoadStreamStatus::kNoResponseBody:
      case LoadStreamStatus::kCannotLoadFromNetworkOffline:
      case LoadStreamStatus::kCannotLoadFromNetworkThrottled:
        zero_state_type = feedui::ZeroStateSlice::CANT_REFRESH;
        break;
      default:
        break;
    }
    // Note that with multiple surface, it's possible that we send a zero-state
    // to a single surface multiple times.
    for (SurfaceInterface& surface : *surfaces_) {
      SendZeroStateUpdate(zero_state_type, &surface);
    }
  }

  // Returns the 0-based index of the slice in the stream, or -1 if the slice is
  // not found. Ignores all non-content slices.
  int GetSliceIndexFromSliceId(const std::string& slice_id) {
    ContentRevision slice_rev = ToContentRevision(slice_id);
    if (slice_rev.is_null())
      return -1;
    int index = 0;
    for (const ContentRevision& rev : model_->GetContentList()) {
      if (rev == slice_rev)
        return index;
      ++index;
    }
    return -1;
  }

 private:
  static feedui::StreamUpdate GetUpdateForNewSurface(const StreamModel& model) {
    feedui::StreamUpdate result;
    for (ContentRevision content_revision : model.GetContentList()) {
      AddSliceUpdate(model, content_revision, /*is_content_new=*/true, &result);
    }
    for (std::string& id : model.GetSharedStateIds()) {
      AddSharedState(model, id, &result);
    }

    return result;
  }

  static void SendLoadingSpinnerUpdate(SurfaceInterface* surface) {
    feedui::StreamUpdate update;
    feedui::Slice* slice = update.add_updated_slices()->mutable_slice();
    slice->mutable_loading_spinner_slice()->set_is_at_top(true);
    slice->set_slice_id("loading-spinner");
    surface->StreamUpdate(update);
  }

  static void SendZeroStateUpdate(feedui::ZeroStateSlice::Type zero_state_type,
                                  SurfaceInterface* surface) {
    feedui::StreamUpdate update;
    feedui::Slice* slice = update.add_updated_slices()->mutable_slice();
    slice->mutable_zero_state_slice()->set_type(zero_state_type);
    slice->set_slice_id(kZeroStateSliceId);
    surface->StreamUpdate(update);
  }

  static void AddSharedState(const StreamModel& model,
                             const std::string& shared_state_id,
                             feedui::StreamUpdate* stream_update) {
    const std::string* shared_state_data =
        model.FindSharedStateData(shared_state_id);
    if (!shared_state_data)
      return;
    feedui::SharedState* added_shared_state =
        stream_update->add_new_shared_states();
    added_shared_state->set_id(shared_state_id);
    added_shared_state->set_xsurface_shared_state(*shared_state_data);
  }

  static void AddSliceUpdate(const StreamModel& model,
                             ContentRevision content_revision,
                             bool is_content_new,
                             feedui::StreamUpdate* stream_update) {
    if (is_content_new) {
      feedui::Slice* slice =
          stream_update->add_updated_slices()->mutable_slice();
      slice->set_slice_id(ToString(content_revision));
      const feedstore::Content* content = model.FindContent(content_revision);
      DCHECK(content);
      slice->mutable_xsurface_slice()->set_xsurface_frame(content->frame());
    } else {
      stream_update->add_updated_slices()->set_slice_id(
          ToString(content_revision));
    }
  }

  // Owned by |FeedStream|.
  // Warning!: Null when the model is not yet loaded.
  StreamModel* model_ = nullptr;
  const base::ObserverList<SurfaceInterface>* surfaces_;

  std::set<ContentRevision> current_content_set_;
};

std::unique_ptr<StreamModelUpdateRequest>
FeedStream::WireResponseTranslator::TranslateWireResponse(
    feedwire::Response response,
    StreamModelUpdateRequest::Source source,
    base::TimeDelta response_time,
    base::Time current_time) {
  return ::feed::TranslateWireResponse(std::move(response), source,
                                       response_time, current_time);
}

FeedStream::FeedStream(RefreshTaskScheduler* refresh_task_scheduler,
                       MetricsReporter* metrics_reporter,
                       Delegate* delegate,
                       PrefService* profile_prefs,
                       FeedNetwork* feed_network,
                       FeedStore* feed_store,
                       const base::Clock* clock,
                       const base::TickClock* tick_clock,
                       const ChromeInfo& chrome_info)
    : refresh_task_scheduler_(refresh_task_scheduler),
      metrics_reporter_(metrics_reporter),
      delegate_(delegate),
      profile_prefs_(profile_prefs),
      feed_network_(feed_network),
      store_(feed_store),
      clock_(clock),
      tick_clock_(tick_clock),
      chrome_info_(chrome_info),
      task_queue_(this),
      user_classifier_(std::make_unique<UserClassifier>(profile_prefs, clock)),
      request_throttler_(profile_prefs, clock) {
  static WireResponseTranslator default_translator;
  wire_response_translator_ = &default_translator;

  surface_updater_ = std::make_unique<SurfaceUpdater>(&surfaces_);

  // Inserting this task first ensures that |store_| is initialized before
  // it is used.
  task_queue_.AddTask(std::make_unique<WaitForStoreInitializeTask>(store_));
}

void FeedStream::InitializeScheduling() {
  if (!IsArticlesListVisible()) {
    refresh_task_scheduler_->Cancel();
    return;
  }

  refresh_task_scheduler_->EnsureScheduled(
      GetUserClassTriggerThreshold(GetUserClass(), TriggerType::kFixedTimer));
}

FeedStream::~FeedStream() = default;

void FeedStream::TriggerStreamLoad() {
  if (model_ || model_loading_in_progress_)
    return;

  // If we should not load the stream, abort and send a zero-state update.
  LoadStreamStatus do_not_attempt_reason = ShouldAttemptLoad();
  if (do_not_attempt_reason != LoadStreamStatus::kNoStatus) {
    InitialStreamLoadComplete(LoadStreamTask::Result(do_not_attempt_reason));
    return;
  }

  model_loading_in_progress_ = true;
  surface_updater_->LoadStreamStarted();
  task_queue_.AddTask(std::make_unique<LoadStreamTask>(
      LoadStreamTask::LoadType::kInitialLoad, this,
      base::BindOnce(&FeedStream::InitialStreamLoadComplete,
                     base::Unretained(this))));
}

void FeedStream::InitialStreamLoadComplete(LoadStreamTask::Result result) {
  metrics_reporter_->OnLoadStream(result.load_from_store_status,
                                  result.final_status);

  model_loading_in_progress_ = false;

  // If loading failed, update surfaces with an appropriate zero-state error.
  if (!model_) {
    surface_updater_->LoadStreamFailed(result.final_status);
  }
}

void FeedStream::AttachSurface(SurfaceInterface* surface) {
  metrics_reporter_->SurfaceOpened();
  user_classifier_->OnEvent(UserClassifier::Event::kSuggestionsViewed);
  surfaces_.AddObserver(surface);
  surface_updater_->SurfaceAdded(surface);
  TriggerStreamLoad();
}

void FeedStream::DetachSurface(SurfaceInterface* surface) {
  surfaces_.RemoveObserver(surface);
}

void FeedStream::SetArticlesListVisible(bool is_visible) {
  profile_prefs_->SetBoolean(prefs::kArticlesListVisible, is_visible);
}

bool FeedStream::IsArticlesListVisible() {
  return profile_prefs_->GetBoolean(prefs::kArticlesListVisible);
}

void FeedStream::LoadMore(base::OnceCallback<void(bool)> callback) {
  if (!model_) {
    DLOG(ERROR) << "Ignoring LoadMore() before the model is loaded";
    return std::move(callback).Run(false);
  }
  // Have at most one in-flight LoadMore() request. Send the result to all
  // requestors.
  load_more_complete_callbacks_.push_back(std::move(callback));
  if (load_more_complete_callbacks_.size() == 1) {
    task_queue_.AddTask(std::make_unique<LoadMoreTask>(
        this,
        base::BindOnce(&FeedStream::LoadMoreComplete, base::Unretained(this))));
  }
}

void FeedStream::LoadMoreComplete(LoadMoreTask::Result result) {
  metrics_reporter_->OnLoadMore(result.final_status);
  std::vector<base::OnceCallback<void(bool)>> moved_callbacks =
      std::move(load_more_complete_callbacks_);
  bool success = result.final_status == LoadStreamStatus::kLoadedFromNetwork;
  for (auto& callback : moved_callbacks) {
    std::move(callback).Run(success);
  }
}

void FeedStream::ExecuteOperations(
    std::vector<feedstore::DataOperation> operations) {
  if (!model_) {
    DLOG(ERROR) << "Calling ExecuteOperations before the model is loaded";
    return;
  }
  return model_->ExecuteOperations(std::move(operations));
}

EphemeralChangeId FeedStream::CreateEphemeralChange(
    std::vector<feedstore::DataOperation> operations) {
  if (!model_) {
    DLOG(ERROR) << "Calling CreateEphemeralChange before the model is loaded";
    return {};
  }
  return model_->CreateEphemeralChange(std::move(operations));
}

bool FeedStream::CommitEphemeralChange(EphemeralChangeId id) {
  if (!model_)
    return false;
  return model_->CommitEphemeralChange(id);
}

bool FeedStream::RejectEphemeralChange(EphemeralChangeId id) {
  if (!model_)
    return false;
  return model_->RejectEphemeralChange(id);
}

UserClass FeedStream::GetUserClass() {
  return user_classifier_->GetUserClass();
}

base::Time FeedStream::GetLastFetchTime() {
  const base::Time fetch_time =
      profile_prefs_->GetTime(feed::prefs::kLastFetchAttemptTime);
  // Ignore impossible time values.
  if (fetch_time > clock_->Now())
    return base::Time();
  return fetch_time;
}

void FeedStream::LoadModelForTesting(std::unique_ptr<StreamModel> model) {
  LoadModel(std::move(model));
}
offline_pages::TaskQueue* FeedStream::GetTaskQueueForTesting() {
  return &task_queue_;
}

void FeedStream::OnTaskQueueIsIdle() {
  if (idle_callback_)
    idle_callback_.Run();
}

void FeedStream::SetIdleCallbackForTesting(
    base::RepeatingClosure idle_callback) {
  idle_callback_ = idle_callback;
}

void FeedStream::SetUserClassifierForTesting(
    std::unique_ptr<UserClassifier> user_classifier) {
  user_classifier_ = std::move(user_classifier);
}

void FeedStream::OnStoreChange(StreamModel::StoreUpdate update) {
  if (!update.operations.empty()) {
    DCHECK(!update.update_request);
    store_->WriteOperations(update.sequence_number, update.operations);
  } else {
    DCHECK(update.update_request);
    if (update.overwrite_stream_data) {
      DCHECK_EQ(update.sequence_number, 0);
      store_->OverwriteStream(std::move(update.update_request),
                              base::DoNothing());
    } else {
      store_->SaveStreamUpdate(update.sequence_number,
                               std::move(update.update_request),
                               base::DoNothing());
    }
  }
}

LoadStreamStatus FeedStream::ShouldAttemptLoad(bool model_loading) {
  // Don't try to load the model if it's already loaded, or in the process of
  // being loaded. Because |ShouldAttemptLoad()| is used both before and during
  // the load process, we need to ignore this check when |model_loading| is
  // true.
  if (model_ || (!model_loading && model_loading_in_progress_))
    return LoadStreamStatus::kModelAlreadyLoaded;

  if (!IsArticlesListVisible())
    return LoadStreamStatus::kLoadNotAllowedArticlesListHidden;

  if (!delegate_->IsEulaAccepted())
    return LoadStreamStatus::kLoadNotAllowedEulaNotAccepted;

  return LoadStreamStatus::kNoStatus;
}

LoadStreamStatus FeedStream::ShouldMakeFeedQueryRequest(bool is_load_more) {
  if (!is_load_more) {
    // Time has passed since calling |ShouldAttemptLoad()|, call it again to
    // confirm we should still attempt loading.
    const LoadStreamStatus should_not_attempt_reason =
        ShouldAttemptLoad(/*model_loading=*/true);
    if (should_not_attempt_reason != LoadStreamStatus::kNoStatus) {
      return should_not_attempt_reason;
    }
  }

  // TODO(harringtond): |suppress_refreshes_until_| was historically used
  // for privacy purposes after clearing data to make sure sync data made it
  // to the server. I'm not sure we need this now. But also, it was
  // documented as not affecting manually triggered refreshes, but coded in
  // a way that it does. I've tried to keep the same functionality as the
  // old feed code, but we should revisit this.
  if (tick_clock_->NowTicks() < suppress_refreshes_until_) {
    return LoadStreamStatus::kCannotLoadFromNetworkSupressedForHistoryDelete;
  }

  if (delegate_->IsOffline()) {
    return LoadStreamStatus::kCannotLoadFromNetworkOffline;
  }

  if (!request_throttler_.RequestQuota(NetworkRequestType::kFeedQuery)) {
    return LoadStreamStatus::kCannotLoadFromNetworkThrottled;
  }

  return LoadStreamStatus::kNoStatus;
}

void FeedStream::OnEulaAccepted() {
  if (surfaces_.might_have_observers())
    TriggerStreamLoad();
}

void FeedStream::OnHistoryDeleted() {
  // Due to privacy, we should not fetch for a while (unless the user
  // explicitly asks for new suggestions) to give sync the time to propagate
  // the changes in history to the server.
  suppress_refreshes_until_ =
      tick_clock_->NowTicks() + kSuppressRefreshDuration;
  ClearAll();
}

void FeedStream::OnCacheDataCleared() {
  ClearAll();
}

void FeedStream::OnSignedIn() {
  ClearAll();
}

void FeedStream::OnSignedOut() {
  ClearAll();
}

void FeedStream::OnEnterForeground() {
  // The v1 feed may trigger a refresh on foregrounding,
  // but I don't think that makes any sense here. We already refresh the feed
  // if necessary when the stream loads.
}

void FeedStream::ExecuteRefreshTask() {
  LoadStreamStatus do_not_attempt_reason = ShouldAttemptLoad();
  if (do_not_attempt_reason != LoadStreamStatus::kNoStatus) {
    BackgroundRefreshComplete(LoadStreamTask::Result(do_not_attempt_reason));
    return;
  }

  task_queue_.AddTask(std::make_unique<LoadStreamTask>(
      LoadStreamTask::LoadType::kBackgroundRefresh, this,
      base::BindOnce(&FeedStream::BackgroundRefreshComplete,
                     base::Unretained(this))));
}

void FeedStream::BackgroundRefreshComplete(LoadStreamTask::Result result) {
  metrics_reporter_->OnBackgroundRefresh(result.final_status);
  refresh_task_scheduler_->RefreshTaskComplete();
}

void FeedStream::ClearAll() {
  metrics_reporter_->OnClearAll(clock_->Now() - GetLastFetchTime());

  // TODO(harringtond): This should result in clearing feed data
  // and triggering a model load attempt if there are connected surfaces.
  // That work should be embedded in a task.
}

void FeedStream::LoadModel(std::unique_ptr<StreamModel> model) {
  DCHECK(!model_);
  model_ = std::move(model);
  model_->SetStoreObserver(this);
  surface_updater_->SetModel(model_.get());
}

void FeedStream::UnloadModel() {
  // Note: This should only be called from a running Task, as some tasks assume
  // the model remains loaded.
  if (!model_)
    return;
  surface_updater_->SetModel(nullptr);
  model_.reset();
}

void FeedStream::ReportOpenAction() {
  user_classifier_->OnEvent(UserClassifier::Event::kSuggestionsUsed);
  metrics_reporter_->OpenAction();
}
void FeedStream::ReportOpenInNewTabAction() {
  user_classifier_->OnEvent(UserClassifier::Event::kSuggestionsUsed);
  metrics_reporter_->OpenInNewTabAction();
}
void FeedStream::ReportSliceViewed(const std::string& slice_id) {
  int index = surface_updater_->GetSliceIndexFromSliceId(slice_id);
  if (index >= 0)
    metrics_reporter_->ContentSliceViewed(index);
}

void FeedStream::ReportSendFeedbackAction() {
  metrics_reporter_->SendFeedbackAction();
}
void FeedStream::ReportLearnMoreAction() {
  metrics_reporter_->LearnMoreAction();
}
void FeedStream::ReportDownloadAction() {
  metrics_reporter_->DownloadAction();
}
void FeedStream::ReportNavigationStarted() {
  metrics_reporter_->NavigationStarted();
}
void FeedStream::ReportNavigationDone() {
  metrics_reporter_->NavigationDone();
}
void FeedStream::ReportRemoveAction() {
  metrics_reporter_->RemoveAction();
}
void FeedStream::ReportNotInterestedInAction() {
  metrics_reporter_->NotInterestedInAction();
}
void FeedStream::ReportManageInterestsAction() {
  metrics_reporter_->ManageInterestsAction();
}
void FeedStream::ReportContextMenuOpened() {
  metrics_reporter_->ContextMenuOpened();
}
void FeedStream::ReportStreamScrolled(int distance_dp) {
  metrics_reporter_->StreamScrolled(distance_dp);
}

}  // namespace feed
