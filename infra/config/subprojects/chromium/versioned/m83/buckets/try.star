# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load('//lib/builders.star', 'cpu', 'goma', 'os')
load('//lib/try.star', 'try_')
# Load this using relative path so that the load statement doesn't
# need to be changed when making a new milestone
load('../vars.star', 'vars')


try_.declare_bucket(vars)

try_.set_defaults(
    vars,
    main_list_view = vars.main_list_view_name,
)


# Builders are sorted first lexicographically by the function used to define
# them, then lexicographically by their name


try_.blink_builder(
    name = 'linux-blink-rel',
    goma_backend = goma.backend.RBE_PROD,
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/cc/.+',
            '.+/[+]/third_party/blink/renderer/core/paint/.+',
            '.+/[+]/third_party/blink/renderer/core/svg/.+',
            '.+/[+]/third_party/blink/renderer/platform/graphics/.+',
        ],
    ),
)


try_.chromium_android_builder(
    name = 'android-binary-size',
    executable = 'recipe:binary_size_trybot',
    goma_jobs = goma.jobs.J150,
    tryjob = try_.job(),
)

try_.chromium_android_builder(
    name = 'android-cronet-arm-dbg',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/components/cronet/.+',
            '.+/[+]/components/grpc_support/.+',
            '.+/[+]/build/android/.+',
            '.+/[+]/build/config/android/.+',
        ],
        location_regexp_exclude = [
            '.+/[+]/components/cronet/ios/.+',
        ],
    ),
)

try_.chromium_android_builder(
    name = 'android-lollipop-arm-rel',
    goma_jobs = goma.jobs.J150,
    tryjob = try_.job(),
)

try_.chromium_android_builder(
    name = 'android-marshmallow-arm64-rel',
    cores = 16,
    goma_jobs = goma.jobs.J300,
    ssd = True,
    use_java_coverage = True,
    tryjob = try_.job(),
)

try_.chromium_android_builder(
    name = 'android-pie-arm64-dbg',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/chrome/android/features/vr/.+',
            '.+/[+]/chrome/android/java/src/org/chromium/chrome/browser/vr/.+',
            '.+/[+]/chrome/android/javatests/src/org/chromium/chrome/browser/vr/.+',
            '.+/[+]/chrome/browser/vr/.+',
            '.+/[+]/third_party/gvr-android-sdk/.+',
            '.+/[+]/third_party/arcore-android-sdk/.+',
            '.+/[+]/third_party/arcore-android-sdk-client/.+',
        ],
    ),
)

try_.chromium_android_builder(
    name = 'android-pie-arm64-rel',
    cores = 16,
    goma_jobs = goma.jobs.J300,
    ssd = True,
    tryjob = try_.job(),
)

try_.chromium_android_builder(
    name = 'android_compile_dbg',
    goma_jobs = goma.jobs.J150,
    tryjob = try_.job(),
)

try_.chromium_android_builder(
    name = 'android_compile_x64_dbg',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/chrome/android/java/src/org/chromium/chrome/browser/vr/.+',
            '.+/[+]/chrome/browser/vr/.+',
            '.+/[+]/sandbox/linux/seccomp-bpf/.+',
            '.+/[+]/sandbox/linux/seccomp-bpf-helpers/.+',
            '.+/[+]/sandbox/linux/system_headers/.+',
            '.+/[+]/sandbox/linux/tests/.+',
            '.+/[+]/third_party/gvr-android-sdk/.+',
        ],
    ),
)

try_.chromium_android_builder(
    name = 'android_compile_x86_dbg',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/chrome/android/java/src/org/chromium/chrome/browser/vr/.+',
            '.+/[+]/chrome/browser/vr/.+',
            '.+/[+]/sandbox/linux/seccomp-bpf/.+',
            '.+/[+]/sandbox/linux/seccomp-bpf-helpers/.+',
            '.+/[+]/sandbox/linux/system_headers/.+',
            '.+/[+]/sandbox/linux/tests/.+',
            '.+/[+]/third_party/gvr-android-sdk/.+',
        ],
    ),
)

try_.chromium_android_builder(
    name = 'android_cronet',
    tryjob = try_.job(),
)

try_.chromium_android_builder(
    name = 'cast_shell_android',
    tryjob = try_.job(),
)


try_.chromium_chromiumos_builder(
    name = 'chromeos-amd64-generic-dbg',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/content/gpu/.+',
            '.+/[+]/media/.+',
        ],
    ),
)

try_.chromium_chromiumos_builder(
    name = 'chromeos-amd64-generic-rel',
    tryjob = try_.job(),
)

try_.chromium_chromiumos_builder(
    name = 'chromeos-arm-generic-rel',
    tryjob = try_.job(),
)

try_.chromium_chromiumos_builder(
    name = 'linux-chromeos-compile-dbg',
    tryjob = try_.job(),
)

try_.chromium_chromiumos_builder(
    name = 'linux-chromeos-rel',
    goma_jobs = goma.jobs.J150,
    tryjob = try_.job(cancel_stale = False),
    use_clang_coverage = True,
)


try_.chromium_dawn_builder(
    name = 'dawn-linux-x64-deps-rel',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/gpu/.+',
            '.+/[+]/testing/buildbot/chromium.dawn.json',
            '.+/[+]/third_party/blink/renderer/modules/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/external/wpt/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/wpt_internal/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/WebGPUExpectations',
            '.+/[+]/third_party/dawn/.+',
            '.+/[+]/tools/clang/scripts/update.py',
            '.+/[+]/ui/gl/features.gni',
        ],
    ),
)

try_.chromium_dawn_builder(
    name = 'dawn-mac-x64-deps-rel',
    os = os.MAC_ANY,
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/gpu/.+',
            '.+/[+]/testing/buildbot/chromium.dawn.json',
            '.+/[+]/third_party/blink/renderer/modules/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/external/wpt/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/wpt_internal/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/WebGPUExpectations',
            '.+/[+]/third_party/dawn/.+',
            '.+/[+]/tools/clang/scripts/update.py',
            '.+/[+]/ui/gl/features.gni',
        ],
    ),
)

try_.chromium_dawn_builder(
    name = 'dawn-win10-x64-deps-rel',
    os = os.WINDOWS_ANY,
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/gpu/.+',
            '.+/[+]/testing/buildbot/chromium.dawn.json',
            '.+/[+]/third_party/blink/renderer/modules/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/external/wpt/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/wpt_internal/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/WebGPUExpectations',
            '.+/[+]/third_party/dawn/.+',
            '.+/[+]/tools/clang/scripts/update.py',
            '.+/[+]/ui/gl/features.gni',
        ],
    ),
)

try_.chromium_dawn_builder(
    name = 'dawn-win10-x86-deps-rel',
    os = os.WINDOWS_ANY,
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/gpu/.+',
            '.+/[+]/testing/buildbot/chromium.dawn.json',
            '.+/[+]/third_party/blink/renderer/modules/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/external/wpt/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/wpt_internal/webgpu/.+',
            '.+/[+]/third_party/blink/web_tests/WebGPUExpectations',
            '.+/[+]/third_party/dawn/.+',
            '.+/[+]/tools/clang/scripts/update.py',
            '.+/[+]/ui/gl/features.gni',
        ],
    ),
)


try_.chromium_linux_builder(
    name = 'cast_shell_linux',
    tryjob = try_.job(),
)

try_.chromium_linux_builder(
    name = 'closure_compilation',
    executable = 'recipe:closure_compilation',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/third_party/closure_compiler/.+',
        ],
    ),
)

try_.chromium_linux_builder(
    name = 'chromium_presubmit',
    executable = 'recipe:presubmit',
    goma_backend = None,
    properties = {
        '$depot_tools/presubmit': {
            'runhooks': True,
            'timeout_s': 480,
        },
        'repo_name': 'chromium',
    },
    tryjob = try_.job(
        disable_reuse = True,
    ),
)

try_.chromium_linux_builder(
    name = 'fuchsia-arm64-cast',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/chromecast/.+',
        ],
    ),
)

try_.chromium_linux_builder(
    name = 'fuchsia-x64-cast',
    tryjob = try_.job(),
)

try_.chromium_linux_builder(
    name = 'fuchsia_arm64',
    tryjob = try_.job(),
)

try_.chromium_linux_builder(
    name = 'fuchsia_x64',
    tryjob = try_.job(),
)

try_.chromium_linux_builder(
    name = 'linux-libfuzzer-asan-rel',
    executable = 'recipe:chromium_libfuzzer_trybot',
    tryjob = try_.job(),
)

try_.chromium_linux_builder(
    name = 'linux-ozone-rel',
    tryjob = try_.job(),
)

try_.chromium_linux_builder(
    name = 'linux-rel',
    goma_jobs = goma.jobs.J150,
    tryjob = try_.job(),
    use_clang_coverage = True,
)

try_.chromium_linux_builder(
    name = 'linux_chromium_asan_rel_ng',
    goma_jobs = goma.jobs.J150,
    ssd = True,
    tryjob = try_.job(),
)

try_.chromium_linux_builder(
    name = 'linux_chromium_compile_dbg_ng',
    caches = [
        swarming.cache(
            name = 'builder',
            path = 'linux_debug',
        ),
    ],
    goma_jobs = goma.jobs.J150,
    tryjob = try_.job(),
)

try_.chromium_linux_builder(
    name = 'linux_chromium_dbg_ng',
    caches = [
        swarming.cache(
            name = 'builder',
            path = 'linux_debug',
        ),
    ],
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/build/.*check_gn_headers.*',
        ],
    ),
)

try_.chromium_linux_builder(
    name = 'linux_chromium_tsan_rel_ng',
    goma_jobs = goma.jobs.J150,
    tryjob = try_.job(),
)

try_.chromium_linux_builder(
    name = 'linux_layout_tests_composite_after_paint',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/third_party/blink/renderer/core/paint/.+',
            '.+/[+]/third_party/blink/renderer/core/svg/.+',
            '.+/[+]/third_party/blink/renderer/platform/graphics/.+',
            '.+/[+]/third_party/blink/web_tests/.+',
        ],
    ),
)

try_.chromium_linux_builder(
    name = 'linux_layout_tests_layout_ng_disabled',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/third_party/blink/renderer/core/editing/.+',
            '.+/[+]/third_party/blink/renderer/core/layout/.+',
            '.+/[+]/third_party/blink/renderer/core/paint/.+',
            '.+/[+]/third_party/blink/renderer/core/svg/.+',
            '.+/[+]/third_party/blink/renderer/platform/fonts/shaping/.+',
            '.+/[+]/third_party/blink/renderer/platform/graphics/.+',
            '.+/[+]/third_party/blink/web_tests/FlagExpectations/disable-layout-ng',
            '.+/[+]/third_party/blink/web_tests/flag-specific/disable-layout-ng/.+',
        ],
    ),
)

try_.chromium_linux_builder(
    name = 'linux_vr',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/chrome/browser/vr/.+',
        ],
    ),
)


try_.chromium_mac_builder(
    name = 'mac-rel',
    goma_jobs = goma.jobs.J150,
    os = os.MAC_10_13,
    tryjob = try_.job(),
)

try_.chromium_mac_builder(
    name = 'mac_chromium_compile_dbg_ng',
    goma_jobs = goma.jobs.J150,
    os = os.MAC_10_13,
    tryjob = try_.job(),
)


try_.chromium_mac_ios_builder(
    name = 'ios-simulator',
    tryjob = try_.job(),
)

try_.chromium_mac_ios_builder(
    name = 'ios-simulator-cronet',
    executable = 'recipe:chromium_trybot',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/components/cronet/.+',
            '.+/[+]/components/grpc_support/.+',
            '.+/[+]/ios/.+',
        ],
        location_regexp_exclude = [
            '.+/[+]/components/cronet/android/.+',
        ],
    ),
)

try_.chromium_mac_ios_builder(
    name = 'ios-simulator-full-configs',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/ios/.+',
        ],
    ),
)


try_.chromium_win_builder(
    name = 'win-libfuzzer-asan-rel',
    builderless = False,
    executable = 'recipe:chromium_libfuzzer_trybot',
    os = os.WINDOWS_ANY,
    tryjob = try_.job(cancel_stale = False),
)

try_.chromium_win_builder(
    name = 'win_chromium_compile_dbg_ng',
    goma_jobs = goma.jobs.J150,
    tryjob = try_.job(cancel_stale = False),
)

try_.chromium_win_builder(
    name = 'win10_chromium_x64_rel_ng',
    goma_jobs = goma.jobs.J150,
    os = os.WINDOWS_10,
    ssd = True,
    use_clang_coverage = True,
    tryjob = try_.job(cancel_stale = False),
)


try_.gpu_chromium_android_builder(
    name = 'android_optional_gpu_tests_rel',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/cc/.+',
            '.+/[+]/chrome/browser/vr/.+',
            '.+/[+]/components/viz/.+',
            '.+/[+]/content/test/gpu/.+',
            '.+/[+]/gpu/.+',
            '.+/[+]/media/audio/.+',
            '.+/[+]/media/filters/.+',
            '.+/[+]/media/gpu/.+',
            '.+/[+]/services/viz/.+',
            '.+/[+]/testing/trigger_scripts/.+',
            '.+/[+]/third_party/blink/renderer/modules/webgl/.+',
            '.+/[+]/third_party/blink/renderer/platform/graphics/gpu/.+',
            '.+/[+]/tools/clang/scripts/update.py',
            '.+/[+]/ui/gl/.+',
        ],
    ),
)


try_.gpu_chromium_linux_builder(
    name = 'linux_optional_gpu_tests_rel',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/chrome/browser/vr/.+',
            '.+/[+]/content/test/gpu/.+',
            '.+/[+]/gpu/.+',
            '.+/[+]/media/audio/.+',
            '.+/[+]/media/filters/.+',
            '.+/[+]/media/gpu/.+',
            '.+/[+]/testing/buildbot/chromium.gpu.fyi.json',
            '.+/[+]/testing/trigger_scripts/.+',
            '.+/[+]/third_party/blink/renderer/modules/webgl/.+',
            '.+/[+]/third_party/blink/renderer/platform/graphics/gpu/.+',
            '.+/[+]/tools/clang/scripts/update.py',
            '.+/[+]/ui/gl/.+',
        ],
    ),
)


try_.gpu_chromium_mac_builder(
    name = 'mac_optional_gpu_tests_rel',
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/chrome/browser/vr/.+',
            '.+/[+]/content/test/gpu/.+',
            '.+/[+]/gpu/.+',
            '.+/[+]/media/audio/.+',
            '.+/[+]/media/filters/.+',
            '.+/[+]/media/gpu/.+',
            '.+/[+]/services/shape_detection/.+',
            '.+/[+]/testing/buildbot/chromium.gpu.fyi.json',
            '.+/[+]/testing/trigger_scripts/.+',
            '.+/[+]/third_party/blink/renderer/modules/webgl/.+',
            '.+/[+]/third_party/blink/renderer/platform/graphics/gpu/.+',
            '.+/[+]/tools/clang/scripts/update.py',
            '.+/[+]/ui/gl/.+',
        ],
    ),
)


try_.gpu_chromium_win_builder(
    name = 'win_optional_gpu_tests_rel',
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    tryjob = try_.job(
        location_regexp = [
            '.+/[+]/chrome/browser/vr/.+',
            '.+/[+]/content/test/gpu/.+',
            '.+/[+]/device/vr/.+',
            '.+/[+]/gpu/.+',
            '.+/[+]/media/audio/.+',
            '.+/[+]/media/filters/.+',
            '.+/[+]/media/gpu/.+',
            '.+/[+]/testing/buildbot/chromium.gpu.fyi.json',
            '.+/[+]/testing/trigger_scripts/.+',
            '.+/[+]/third_party/blink/renderer/modules/vr/.+',
            '.+/[+]/third_party/blink/renderer/modules/webgl/.+',
            '.+/[+]/third_party/blink/renderer/modules/xr/.+',
            '.+/[+]/third_party/blink/renderer/platform/graphics/gpu/.+',
            '.+/[+]/tools/clang/scripts/update.py',
            '.+/[+]/ui/gl/.+',
        ],
    ),
)
