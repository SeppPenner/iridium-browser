// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-lite.js';
import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
import './print_management_shared_css.js';
import './printing_manager.mojom-lite.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getMetadataProvider} from './mojo_interface_provider.js';
import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {FocusRowBehavior} from 'chrome://resources/js/cr/ui/focus_row_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import './strings.js';


/**
 * Converts a mojo time to a JS time.
 * @param {!mojoBase.mojom.Time} mojoTime
 * @return {!Date}
 */
function convertMojoTimeToJS(mojoTime) {
  // The JS Date() is based off of the number of milliseconds since the
  // UNIX epoch (1970-01-01 00::00:00 UTC), while |internalValue| of the
  // base::Time (represented in mojom.Time) represents the number of
  // microseconds since the Windows FILETIME epoch (1601-01-01 00:00:00 UTC).
  // This computes the final JS time by computing the epoch delta and the
  // conversion from microseconds to milliseconds.
  const windowsEpoch = Date.UTC(1601, 0, 1, 0, 0, 0, 0);
  const unixEpoch = Date.UTC(1970, 0, 1, 0, 0, 0, 0);
  // |epochDeltaInMs| equals to base::Time::kTimeTToMicrosecondsOffset.
  const epochDeltaInMs = unixEpoch - windowsEpoch;
  const timeInMs = Number(mojoTime.internalValue) / 1000;

  return new Date(timeInMs - epochDeltaInMs);
};

/**
 * Returns true if |date| is today, false otherwise.
 * @param {!Date} date
 * @return {boolean}
 */
function isToday(date) {
  const today_date = new Date();
  return date.getDate() === today_date.getDate() &&
         date.getMonth() === today_date.getMonth() &&
         date.getFullYear() === today_date.getFullYear();
};

/**
 * @fileoverview
 * 'print-job-entry' is contains a single print job entry and is used as a list
 * item.
 */
Polymer({
  is: 'print-job-entry',

  _template: html`{__html_template__}`,

  behaviors: [
    FocusRowBehavior,
  ],

  /**
   * @private {
   *  ?chromeos.printing.printingManager.mojom.PrintingMetadataProviderInterface
   * }
   */
  mojoInterfaceProvider_: null,

  properties: {
    /** @type {!chromeos.printing.printingManager.mojom.PrintJobInfo} */
    jobEntry: {
      type: Object,
    },

    /** @private */
    jobTitle_: {
      type: String,
      computed: 'decodeString16_(jobEntry.title)',
    },

    /** @private */
    printerName_: {
      type: String,
      computed: 'decodeString16_(jobEntry.printerName)',
    },

    /** @private */
    creationTime_: {
      type: String,
      computed: 'computeDate_(jobEntry.creationTime)'
    },

    /** @private */
    completionStatus_: {
      type: String,
      computed: 'computeCompletionStatus_(jobEntry.completedInfo)',
    },

    /**
     * A representation in fraction form of pages printed versus total number
     * of pages to be printed. E.g. 5/7 (5 pages printed / 7 total pages to
     * print).
     * @private
     */
    readableProgress_: {
      type: String,
      computed: 'computeReadableProgress_(jobEntry.activePrintJobInfo)',
    },

    /** @private */
    jobEntryAriaLabel_: {
      type: String,
      computed: 'getJobEntryAriaLabel_(jobEntry, jobTitle_, printerName_, ' +
          'creationTime_, completionStatus_, ' +
          'jobEntry.activePrintJobinfo.printedPages, jobEntry.numberOfPages)',
    }
  },

  /** @override */
  created() {
    this.mojoInterfaceProvider_ = getMetadataProvider();
  },

  /**
   * @return {string}
   * @private
   */
  computeCompletionStatus_() {
    if (!this.jobEntry.completedInfo) {
      return '';
    }

    return this.convertStatusToString_(
        this.jobEntry.completedInfo.completionStatus);
  },

  /**
   * @return {string}
   * @private
   */
  computeReadableProgress_() {
    if (!this.jobEntry.activePrintJobInfo) {
      return '';
    }

    return loadTimeData.getStringF('printedPagesFraction',
        this.jobEntry.activePrintJobInfo.printedPages.toString(),
        this.jobEntry.numberOfPages.toString());
  },

  /** @private */
  onCancelPrintJobClicked_() {
    this.mojoInterfaceProvider_.cancelPrintJob(this.jobEntry.id).then(
      (/** @param {{attemptedCancel: boolean}} response */(response) =>
          this.onPrintJobCanceled_(response.attemptedCancel)));
  },

  /**
   * @param {boolean} attemptedCancel
   * @private
   */
  onPrintJobCanceled_(attemptedCancel) {
    // TODO(crbug/1093527): Handle error case in which attempted cancellation
    // failed. Need to discuss with UX on error states.
    this.fire('remove-print-job', this.jobEntry.id);
  },

  /**
   * Converts utf16 to a readable string.
   * @param {!mojoBase.mojom.String16} arr
   * @return {string}
   * @private
   */
  decodeString16_(arr) {
    return arr.data.map(ch => String.fromCodePoint(ch)).join('');
  },

  /**
   * Converts mojo time to JS time. Returns "Today" if |mojoTime| is at the
   * current day.
   * @param {!mojoBase.mojom.Time} mojoTime
   * @return {string}
   * @private
   */
  computeDate_(mojoTime) {
    const jsDate = convertMojoTimeToJS(mojoTime);
    // Date() is constructed with the current time in UTC. If the Date() matches
    // |jsDate|'s date, display the 12hour time of the current date.
    if (isToday(jsDate)) {
      return jsDate.toLocaleTimeString(/*locales=*/undefined,
        {hour12: true, hour: 'numeric', minute: 'numeric'});
    }
    // Remove the day of the week from the date.
    return jsDate.toLocaleDateString(/*locales=*/undefined,
        {month: 'short', day: 'numeric', year: 'numeric'});
  },

  /**
   * Returns the corresponding completion status from |mojoCompletionStatus|.
   * @param {number} mojoCompletionStatus
   * @return {string}
   * @private
   */
  convertStatusToString_(mojoCompletionStatus) {
    switch (mojoCompletionStatus) {
      case chromeos.printing.printingManager.mojom.PrintJobCompletionStatus
           .kFailed:
        return loadTimeData.getString('completionStatusFailed');
      case chromeos.printing.printingManager.mojom.PrintJobCompletionStatus
           .kCanceled:
        return loadTimeData.getString('completionStatusCanceled');
      case chromeos.printing.printingManager.mojom.PrintJobCompletionStatus
           .kPrinted:
        return loadTimeData.getString('completionStatusPrinted');
      default:
        assertNotReached();
        return loadTimeData.getString('completionStatusUnknownError');
    }
  },

  /**
   * @return {boolean} Returns true if the job entry is a completed print job.
   *                   Returns false otherwise.
   * @private
   */
  isCompletedPrintJob_() {
    return !!this.jobEntry.completedInfo && !this.jobEntry.activePrintJobInfo;
  },

  /**
   * @return {string}
   * @private
   */
  getJobEntryAriaLabel_() {
    if (!this.jobEntry || !this.jobEntry.numberOfPages != undefined ||
        !this.printerName_ != undefined || !this.jobTitle_ != undefined ||
        !this.creationTime_) {
      return '';
    }

    // |completionStatus_| and |jobEntry.activePrintJobInfo| are mutually
    // exclusive and one of which has to be non-null. Assert that if
    // |completionStatus_| is non-null that |jobEntry.activePrintJobInfo| is
    // null and vice-versa.
    assert(this.completionStatus_ ?
        !this.jobEntry.activePrintJobInfo : this.jobEntry.activePrintJobInfo);

    if (this.isCompletedPrintJob_()) {
      return loadTimeData.getStringF('completePrintJobLabel', this.jobTitle_,
          this.printerName_, this.creationTime_, this.completionStatus_);
    }
    return loadTimeData.getStringF('ongoingPrintJobLabel', this.jobTitle_,
          this.printerName_, this.creationTime_,
          this.jobEntry.activePrintJobInfo.printedPages.toString(),
          this.jobEntry.numberOfPages.toString());
  },

  /**
   * Returns the percentage, out of 100, of the pages printed versus total
   * number of pages.
   * @param {number} printedPages
   * @param {number} totalPages
   * @return {number}
   * @private
   */
  computePrintPagesProgress_(printedPages, totalPages) {
    assert(printedPages >= 0);
    assert(totalPages > 0);
    assert(printedPages <= totalPages);
    return (printedPages * 100) / totalPages;
  },
});
