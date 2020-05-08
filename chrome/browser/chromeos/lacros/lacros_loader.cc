// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/lacros/lacros_loader.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "chrome/browser/component_updater/cros_component_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/upstart/upstart_client.h"

using component_updater::CrOSComponentManager;

namespace {
LacrosLoader* g_instance = nullptr;
}  // namespace

// static
LacrosLoader* LacrosLoader::Get() {
  return g_instance;
}

LacrosLoader::LacrosLoader(scoped_refptr<CrOSComponentManager> manager)
    : cros_component_manager_(manager) {
  DCHECK(cros_component_manager_);

  DCHECK(!g_instance);
  g_instance = this;
}

LacrosLoader::~LacrosLoader() {
  // Try to kill the lacros-chrome binary.
  if (lacros_process_.IsValid())
    lacros_process_.Terminate(/*ignored=*/0, /*wait=*/false);

  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void LacrosLoader::Init() {
  const char kLacrosComponentName[] = "lacros-fishfood";
  if (chromeos::features::IsLacrosComponentUpdaterEnabled()) {
    // TODO(crbug.com/1078607): Remove non-error logging from this class.
    LOG(WARNING) << "Starting lacros component load.";
    cros_component_manager_->Load(kLacrosComponentName,
                                  CrOSComponentManager::MountPolicy::kMount,
                                  CrOSComponentManager::UpdatePolicy::kForce,
                                  base::BindOnce(&LacrosLoader::OnLoadComplete,
                                                 weak_factory_.GetWeakPtr()));
  } else {
    // Clean-up code is currently blocked on fixing the implementation of
    // IsRegistered. https://crbug.com/(1077348)
    // if (cros_component_manager_->IsRegistered(kLacrosComponentName)) {
    // Clean up any old disk images, since the user turned off the flag.
    // cros_component_manager_->Unload(kLacrosComponentName);
    // }
  }
}

void LacrosLoader::Start() {
  std::string chrome_path;
  if (chromeos::features::IsLacrosComponentUpdaterEnabled()) {
    if (lacros_path_.empty()) {
      LOG(WARNING) << "lacros component image not yet available";
      return;
    }
    chrome_path = lacros_path_.MaybeAsASCII() + "/chrome";
  } else {
    // This is the default path on eve-lacros images.
    chrome_path = "/usr/local/lacros/chrome";
  }
  LOG(WARNING) << "Launching lacros-chrome at " << chrome_path;

  base::LaunchOptions options;
  options.environment["EGL_PLATFORM"] = "surfaceless";
  options.environment["XDG_RUNTIME_DIR"] = "/run/chrome";
  options.kill_on_parent_death = true;

  std::vector<std::string> argv = {chrome_path,
                                   "--ozone-platform=wayland",
                                   "--user-data-dir=/home/chronos/user/lacros",
                                   "--enable-gpu-rasterization",
                                   "--enable-oop-rasterization",
                                   "--lang=en-US",
                                   "--breakpad-dump-location=/tmp"};
  lacros_process_ = base::LaunchProcess(argv, options);
  LOG(WARNING) << "Launched lacros-chrome with pid " << lacros_process_.Pid();
}

void LacrosLoader::OnLoadComplete(
    component_updater::CrOSComponentManager::Error error,
    const base::FilePath& path) {
  if (error != CrOSComponentManager::Error::NONE) {
    LOG(WARNING) << "Error loading lacros component image: "
                 << static_cast<int>(error);
    return;
  }
  lacros_path_ = path;
  LOG(WARNING) << "Loaded lacros image at " << lacros_path_.MaybeAsASCII();
}
