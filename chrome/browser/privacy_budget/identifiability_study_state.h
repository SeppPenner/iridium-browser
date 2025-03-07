// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BUDGET_IDENTIFIABILITY_STUDY_STATE_H_
#define CHROME_BROWSER_PRIVACY_BUDGET_IDENTIFIABILITY_STUDY_STATE_H_

#include "base/containers/mru_cache.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece_forward.h"
#include "base/thread_annotations.h"
#include "chrome/browser/privacy_budget/privacy_budget_prefs.h"
#include "chrome/common/privacy_budget/privacy_budget_settings_provider.h"
#include "components/prefs/pref_service.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

namespace test_utils {
class InspectableIdentifiabilityStudySettings;
}  // namespace test_utils

// Current state of the identifiability study.
//
// Persists mutable state in a `PrefService`. This state includes:
//
// * Which identifiable surfaces are "active". I.e. currently being recorded.
//
// * Which identifiable surfaces are "retired". I.e. was active at some point
//   but was demoted due to some reason. Typically this happens if an active
//   surface is blocked by a settings change. These are kept around in order to
//   minimize the total number of surfaces that we record per client.
//
// * The PRNG seed that we use for various pseudo-random operations.
//
// * The study generation.
class IdentifiabilityStudyState {
 public:
  // Construct from a PrefService. `pref_service` is used to retrieve and store
  // study state.
  explicit IdentifiabilityStudyState(PrefService* pref_service);

  IdentifiabilityStudyState(IdentifiabilityStudyState&) = delete;
  IdentifiabilityStudyState& operator=(const IdentifiabilityStudyState&) =
      delete;

  ~IdentifiabilityStudyState();

  // True if this client is included in the identifiability study.
  //
  // A client (i.e. a browser instance) participates in the study if all of the
  // following are true:
  //   * `kIdentifiabilityStudy` feature is enabled.
  //   * `kIdentifiabilityStudyMaxSurfaces` is non-zero.
  //
  // In addition, none of this is relevant if UKM collection is disabled for the
  // client. If that was the case, this class would not be instantiated in the
  // first place.
  bool IsActive() const { return settings_.IsActive(); }

  // Returns the active experiment generation as defined by the server-side
  // configuration.
  //
  // The study generation is specified as part of the study configuration
  // fetched from the server. This number updates each time the experiment
  // parameters change, and can be used to separate clients using different sets
  // of study settings.
  int generation() const;

  // Returns true if metrics collection is enabled for `surface`.
  //
  // Calling this method may alter the state of the study settings.
  bool ShouldSampleSurface(blink::IdentifiableSurface surface);

  // Returns true if the `surface` is blocked by configuration. Does not take
  // into account whether the surface is included in the study or not.
  bool IsSurfaceBlocked(blink::IdentifiableSurface surface) const;

  // A knob that we can use to split data sets from different versions of the
  // implementation where the differences could have material effects on the
  // data distribution.
  //
  // Should be incremented whenever a non-backwards-compatible change is made in
  // the code. This value is independent of any server controlled study
  // parameters.
  static constexpr int kGeneratorVersion = 1;

 private:
  friend class test_utils::InspectableIdentifiabilityStudySettings;

  using IdentifiableSurfaceSet =
      PrivacyBudgetSettingsProvider::IdentifiableSurfaceSet;
  using IdentifiableSurfaceTypeSet =
      PrivacyBudgetSettingsProvider::IdentifiableSurfaceTypeSet;
  using SurfaceSelectionRateMap =
      base::flat_map<blink::IdentifiableSurface,
                     int,
                     blink::IdentifiableSurfaceCompLess>;
  using TypeSelectionRateMap =
      base::flat_map<blink::IdentifiableSurface::Type, int>;

  static std::string EncodedValueFromSurfaces(
      const IdentifiableSurfaceSet& surfaces);

  // Checks that the invariants hold. When DCHECK_IS_ON() this call is
  // expensive. Noop otherwise.
  void CheckInvariants() const;

  // Initialize from fields persisted in `pref_service_`.
  void InitFromPrefs();

  // Write active and retired lists to `pref_service_`.
  void WriteToPrefs();

  // Invoked at the start of the session after loading persisted active and
  // retired lists. It verifies the following:
  //
  // * There should be no blocked surfaces in the active list.
  // * The active list should conform to the restrictions on the number of
  //   allowed surfaces.
  void ReconcileLoadedPrefs();

  // Clears all client-side persisted state. This should only be invoked when
  // the experiment generation changes, or if the UKM client ID has been reset.
  void ResetClientState();

  // Contains all the logic for determining whether a newly observed surface
  // should be added to the active list or not.
  bool DecideSurfaceInclusion(blink::IdentifiableSurface surface);

  // Used for persisting active and retired lists.
  PrefService* pref_service_;

  // Contains kIdentifiabilityStudyGeneration. After the constructor runs, this
  // will also be the study generation corresponding to persisted state.
  const int generation_ = 0;

  // Seed for PRNG. This gets reset each time the study generation changes or if
  // the UKM client ID rolls over.
  uint64_t prng_seed_ = 0;

  // Set of identifiable surfaces for which we will collect metrics. This set is
  // updated as we go unless it is already saturated.
  //
  // The set is considered saturated when the size is `max_active_surfaces_`.
  //
  // Invariants:
  //
  //   * active_surfaces_ ∩ settings_.blocked_surfaces() = Ø.
  //
  //   * active_surfaces_.GetType() ∩ settings_.blocked_types() = Ø.
  //
  //   * active_surfaces_ ∩ retired_surfaces_ = Ø.
  //
  //   * active_surfaces_.size() ≤ max_active_surfaces_.
  //
  IdentifiableSurfaceSet active_surfaces_;

  // Set of identifiable surfaces that were once in the `active_surfaces_` set,
  // but are no longer.
  //
  // If `max_active_surfaces_` falls below the size of `allowed_surfaces_`, then
  // rather than drop the extra surfaces, they are moved here. If the
  // `max_active_surfaces_` increases again, then this class moves as many
  // surfaces as viable from this set to `active_surfaces_` subject to
  // `blocked_surfaces_`.
  //
  // This list persists throughout the lifetime of a UKM client ID and counts
  // towards the cumulative number of surfaces that has been associated with a
  // specified client ID. Reuse of retired surfaces is to minimize the total
  // number of surfaces exposed for each client ID.
  //
  // Invariants:
  //
  //   * active_surfaces_ ∩ retired_surfaces_ = Ø.
  IdentifiableSurfaceSet retired_surfaces_;

  // Cache for accelerating lookups for frequently encountered surfaces. The
  // payload is a boolean which is true if the surface is active.
  base::HashingMRUCache<blink::IdentifiableSurface,
                        bool,
                        blink::IdentifiableSurfaceHash>
      recent_surfaces_;

  // Root denominator of the rate of surface selection. Numerator is always 1.
  // Thus, given a new surface s that is not in `active_surfaces_` nor
  // `blocked_surfaces_`, the probability of that surface being added to
  // `active_surfaces_` is:
  //
  //                                                           1
  //     𝛲𝒓(surface s being added to the study) =  ━━━━━━━━━━━━━━━━━━━━━━━
  //                                               surface_selection_rate_
  //
  // If `surface_selection_rate_` is 0, then it is considered the same as a
  // probability of zero.
  const int surface_selection_rate_ = 0;

  // Per surface custom selection rates. The effective selection probability for
  // a surface is the `per_surface_selection_rates_[surface]` if it is defined.
  // Otherwise defaults to `surface_selection_rate_`
  const SurfaceSelectionRateMap per_surface_selection_rates_;

  // Per surface *type* custom selection rates. Interpreted the same as
  // `per_surface_selection_rates_` except the surface type is used for lookup.
  const TypeSelectionRateMap per_type_selection_rates_;

  // Hard cap on the number of identifiable surfaces we will sample per client.
  // This setting can be tweaked experimentally via
  // `kIdentifiabilityStudyMaxSurfaces`.
  //
  // Invariants:
  //
  //   * max_active_surfaces_ ≤ kIdentifiabilityStudyMaxSurfaces.
  const size_t max_active_surfaces_;

  // Overall study state and per-surface and per-type blocking.
  const PrivacyBudgetSettingsProvider settings_;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_PRIVACY_BUDGET_IDENTIFIABILITY_STUDY_STATE_H_
