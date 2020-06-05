// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/policy/device_policy_decoder_chromeos.h"
#include "chrome/browser/chromeos/policy/fuzzer/policy_fuzzer.pb.h"
#include "chrome/browser/policy/configuration_policy_handler_list_factory.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/policy/core/browser/configuration_policy_handler_list.h"
#include "components/policy/core/browser/policy_conversions_client.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/chrome_schema.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/external_data_manager.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_proto_decoders.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/pref_value_map.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

namespace policy {

namespace {

struct Environment {
  Environment() {
    logging::SetMinLogLevel(logging::LOG_FATAL);
    base::CommandLine::Init(0, nullptr);
    chromeos::DBusThreadManager::Initialize();
    policy_handler_list = BuildHandlerList(GetChromeSchema());
  }

  std::unique_ptr<ConfigurationPolicyHandlerList> policy_handler_list;
};

void CheckPolicyToPrefTranslation(const PolicyMap& policy_map,
                                  const Environment& environment) {
  PrefValueMap prefs;
  PolicyErrorMap errors;
  DeprecatedPoliciesSet deprecated_policies;
  environment.policy_handler_list->ApplyPolicySettings(
      policy_map, &prefs, &errors, &deprecated_policies);
}

}  // namespace

DEFINE_PROTO_FUZZER(const PolicyFuzzerProto& proto) {
  static Environment environment;

  if (proto.has_chrome_device_settings()) {
    const enterprise_management::ChromeDeviceSettingsProto&
        chrome_device_settings = proto.chrome_device_settings();
    base::WeakPtr<ExternalDataManager> data_manager;
    PolicyMap policy_map;
    DecodeDevicePolicy(chrome_device_settings, data_manager, &policy_map);

    for (const auto& it : policy_map) {
      const std::string& policy_name = it.first;
      const PolicyMap::Entry& entry = it.second;
      CHECK(entry.value()) << "Policy " << policy_name << " has an empty value";
      CHECK_EQ(entry.scope, POLICY_SCOPE_MACHINE)
          << "Policy " << policy_name << " has not machine scope";
    }

    CheckPolicyToPrefTranslation(policy_map, environment);
  }

  if (proto.has_cloud_policy_settings()) {
    const enterprise_management::CloudPolicySettings& cloud_policy_settings =
        proto.cloud_policy_settings();
    base::WeakPtr<CloudExternalDataManager> cloud_data_manager;
    PolicyMap policy_map;
    DecodeProtoFields(cloud_policy_settings, cloud_data_manager,
                      PolicySource::POLICY_SOURCE_CLOUD,
                      PolicyScope::POLICY_SCOPE_USER, &policy_map);

    for (const auto& it : policy_map) {
      const std::string& policy_name = it.first;
      const PolicyMap::Entry& entry = it.second;
      CHECK(entry.value()) << "Policy " << policy_name << " has an empty value";
      CHECK_EQ(entry.scope, POLICY_SCOPE_USER)
          << "Policy " << policy_name << " has not user scope";
    }

    CheckPolicyToPrefTranslation(policy_map, environment);
  }
}

}  // namespace policy
