// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media/media_feeds_ui.h"

#include "base/bind.h"

#include "base/macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/feeds/media_feeds_converter.h"
#include "chrome/browser/media/feeds/media_feeds_fetcher.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom-shared.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "chrome/browser/media/history/media_history_keyed_service_factory.h"
#include "chrome/browser/media/history/media_history_store.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/dev_ui_browser_resources.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

MediaFeedsUI::MediaFeedsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  // Setup the data source behind chrome://media-feeds.
  std::unique_ptr<content::WebUIDataSource> source(
      content::WebUIDataSource::Create(chrome::kChromeUIMediaFeedsHost));
  source->AddResourcePath("media-data-table.js", IDR_MEDIA_DATA_TABLE_JS);
  source->AddResourcePath("media-feeds.js", IDR_MEDIA_FEEDS_JS);
  source->AddResourcePath(
      "services/media_session/public/mojom/media_session.mojom-lite.js",
      IDR_MEDIA_SESSION_MOJOM_LITE_JS);
  source->AddResourcePath("ui/gfx/geometry/mojom/geometry.mojom-lite.js",
                          IDR_UI_GEOMETRY_MOJOM_LITE_JS);
  source->AddResourcePath(
      "chrome/browser/media/feeds/media_feeds_store.mojom-lite.js",
      IDR_MEDIA_FEEDS_STORE_MOJOM_LITE_JS);
  source->SetDefaultResource(IDR_MEDIA_FEEDS_HTML);
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source.release());
}

WEB_UI_CONTROLLER_TYPE_IMPL(MediaFeedsUI)

MediaFeedsUI::~MediaFeedsUI() = default;

void MediaFeedsUI::BindInterface(
    mojo::PendingReceiver<media_feeds::mojom::MediaFeedsStore> pending) {
  receiver_.Add(this, std::move(pending));
}

void MediaFeedsUI::GetMediaFeeds(GetMediaFeedsCallback callback) {
  GetMediaHistoryService()->GetMediaFeeds(
      media_history::MediaHistoryKeyedService::GetMediaFeedsRequest(),
      std::move(callback));
}

void MediaFeedsUI::GetItemsForMediaFeed(int64_t feed_id,
                                        GetItemsForMediaFeedCallback callback) {
  GetMediaHistoryService()->GetItemsForMediaFeedForDebug(feed_id,
                                                         std::move(callback));
}

void MediaFeedsUI::OnFetchResponse(
    int64_t feed_id,
    FetchMediaFeedCallback callback,
    const schema_org::improved::mojom::EntityPtr& response,
    media_feeds::MediaFeedsFetcher::Status status) {
  std::vector<media_session::MediaImage> logos;
  std::string display_name;
  auto feed_items = media_feeds::GetMediaFeeds(response, &logos, &display_name);

  if (!feed_items.has_value())
    return;

  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      feed_id, std::move(feed_items.value()),
      media_feeds::mojom::FetchResult::kSuccess, false, std::move(logos),
      display_name, std::move(callback));

  fetcher_.reset();
}

void MediaFeedsUI::FetchMediaFeed(int64_t feed_id,
                                  const GURL& url,
                                  FetchMediaFeedCallback callback) {
  if (fetcher_) {
    std::move(callback).Run();
    return;
  }

  fetcher_ = std::make_unique<media_feeds::MediaFeedsFetcher>(
      content::BrowserContext::GetDefaultStoragePartition(
          Profile::FromWebUI(web_ui()))
          ->GetURLLoaderFactoryForBrowserProcess());
  fetcher_->FetchFeed(
      url,
      // Use of unretained is safe because the callback is owned
      // by fetcher_, which will not outlive this.
      base::BindOnce(&MediaFeedsUI::OnFetchResponse, base::Unretained(this),
                     feed_id, std::move(callback)));
}

media_history::MediaHistoryKeyedService*
MediaFeedsUI::GetMediaHistoryService() {
  Profile* profile = Profile::FromWebUI(web_ui());
  DCHECK(profile);

  media_history::MediaHistoryKeyedService* service =
      media_history::MediaHistoryKeyedServiceFactory::GetForProfile(profile);
  DCHECK(service);
  return service;
}
