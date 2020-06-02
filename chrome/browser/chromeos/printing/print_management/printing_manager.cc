// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/print_management/printing_manager.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/printing/history/print_job_history_service.h"
#include "chrome/browser/chromeos/printing/history/print_job_history_service_factory.h"
#include "chrome/browser/chromeos/printing/print_management/print_job_info_mojom_conversions.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {
namespace printing {
namespace print_management {

using chromeos::printing::proto::PrintJobInfo;
using history::DeletionInfo;
using history::HistoryService;
using printing_manager::mojom::PrintingMetadataProvider;
using printing_manager::mojom::PrintJobInfoPtr;
using printing_manager::mojom::PrintJobsObserver;

PrintingManager::PrintingManager(
    PrintJobHistoryService* print_job_history_service,
    HistoryService* history_service)
    : print_job_history_service_(print_job_history_service),
      history_service_(history_service) {
  if (history_service_) {
    history_service_->AddObserver(this);
  }
}

PrintingManager::~PrintingManager() {
  if (history_service_) {
    history_service_->RemoveObserver(this);
  }
}

void PrintingManager::GetPrintJobs(GetPrintJobsCallback callback) {
  print_job_history_service_->GetPrintJobs(
      base::BindOnce(&PrintingManager::OnPrintJobsRetrieved,
                     base::Unretained(this), std::move(callback)));
}

void PrintingManager::DeleteAllPrintJobs(DeleteAllPrintJobsCallback callback) {
  if (IsHistoryDeletionPreventedByPolicy()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  print_job_history_service_->DeleteAllPrintJobs(std::move(callback));
}

void PrintingManager::ObservePrintJobs(
    mojo::PendingRemote<PrintJobsObserver> observer,
    ObservePrintJobsCallback callback) {
  print_job_observers_.Add(std::move(observer));
  std::move(callback).Run();
}

void PrintingManager::OnURLsDeleted(HistoryService* history_service,
                                    const DeletionInfo& deletion_info) {
  // We only handle deletion of all history because it is an explicit action by
  // user to explicitly remove all their history-related content.
  if (IsHistoryDeletionPreventedByPolicy() || !deletion_info.IsAllHistory()) {
    return;
  }

  DeleteAllPrintJobs(base::BindOnce(&PrintingManager::OnPrintJobsDeleted,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void PrintingManager::OnPrintJobsDeleted(bool success) {
  DCHECK(success) << "Clearing print jobs failed unexpectedly.";
  for (auto& observer : print_job_observers_) {
    observer->OnAllPrintJobsDeleted();
  }
}

void PrintingManager::OnPrintJobsRetrieved(
    GetPrintJobsCallback callback,
    bool success,
    std::vector<PrintJobInfo> print_job_info_protos) {
  std::vector<PrintJobInfoPtr> print_job_infos;

  if (success) {
    for (const auto& print_job_info : print_job_info_protos) {
      print_job_infos.push_back(PrintJobProtoToMojom(print_job_info));
    }
  }

  std::move(callback).Run(std::move(print_job_infos));
}

void PrintingManager::BindInterface(
    mojo::PendingReceiver<PrintingMetadataProvider> pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

// TODO(crbug/1053704): Implement the policy and check for the policy here.
bool PrintingManager::IsHistoryDeletionPreventedByPolicy() {
  NOTIMPLEMENTED()
      << "IsHistoryDeletionPreventedByPolicy is not yet implemented";
  return true;
}

void PrintingManager::Shutdown() {
  receiver_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

}  // namespace print_management
}  // namespace printing
}  // namespace chromeos
