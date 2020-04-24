// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.text.TextUtils;

import org.junit.Assert;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.CalledByNativeJavaTest;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Unit tests for {@link OmniboxSuggestion}.
 */
public class OmniboxSuggestionUnitTest {
    @CalledByNative
    private OmniboxSuggestionUnitTest() {}

    /**
     * Compare two cached {@link OmniboxSuggestion} to see if they are same. Only comparing cached
     * fields here Since OmniboxSuggestion::cacheOmniboxSuggestionListForZeroSuggest do not cache
     * all the fields.
     *
     * @return True if two {@link OmniboxSuggestion} are same, otherwise false.
     */
    boolean isCachedOmniboxSuggestionMatch(
            OmniboxSuggestion suggestion1, OmniboxSuggestion suggestion2) {
        return suggestion1.getType() == suggestion2.getType()
                && TextUtils.equals(suggestion1.getDisplayText(), suggestion2.getDisplayText())
                && TextUtils.equals(suggestion1.getDescription(), suggestion2.getDescription())
                && suggestion1.getUrl().equals(suggestion2.getUrl())
                && suggestion1.isSearchSuggestion() == suggestion2.isSearchSuggestion()
                && suggestion1.isStarred() == suggestion2.isStarred()
                && suggestion1.isDeletable() == suggestion2.isDeletable()
                && TextUtils.equals(
                        suggestion1.getPostContentType(), suggestion2.getPostContentType())
                && Arrays.equals(suggestion1.getPostData(), suggestion2.getPostData());
    }

    /**
     * Compare two list of {@link OmniboxSuggestion} to see if they are same.
     *
     * @return True if two list of {@link OmniboxSuggestion} are same, otherwise false.
     */
    boolean isOmniboxSuggestionListMatch(
            List<OmniboxSuggestion> list1, List<OmniboxSuggestion> list2) {
        if (list1.size() != list2.size()) return false;
        for (OmniboxSuggestion suggestion1 : list1) {
            for (OmniboxSuggestion suggestion2 : list2) {
                if (isCachedOmniboxSuggestionMatch(suggestion1, suggestion2)) {
                    list2.remove(suggestion2);
                    break;
                }
            }
        }
        return list2.isEmpty();
    }

    /**
     * Build a dummy suggestions list .
     * @param count How many suggestions to create.
     * @param hasPostData If suggestions contain post data.
     *
     * @return List of suggestions.
     */
    private List<OmniboxSuggestion> buildDummySuggestionsList(int count, boolean hasPostData) {
        List<OmniboxSuggestion> list = new ArrayList<>();

        for (int index = 0; index < count; ++index) {
            OmniboxSuggestion suggestion = new OmniboxSuggestion(
                    OmniboxSuggestionType.CLIPBOARD_IMAGE, false /* isSearchType */,
                    0 /* relevance */, 0 /* transition */, "dummy text 1" + (index + 1),
                    null /* displayTextClassifications */,
                    "dummy description 1" + (index + 1) /* description */,
                    null /* descriptionClassifications */, null /* answer */,
                    null /* fillIntoEdit */, new GURL("dummy url" + (index + 1)) /* url */,
                    GURL.emptyGURL() /* imageUrl */, null /* imageDominantColor */,
                    false /* isStarred */, false /* isDeletable */,
                    hasPostData ? "Dummy Content Type" + (index + 1) : null /* postContentType */,
                    hasPostData ? new byte[] {4, 5, 6, (byte) (index + 1)} : null /* postData */,
                    OmniboxSuggestion.INVALID_GROUP);
            list.add(suggestion);
        }

        return list;
    }

    @CalledByNativeJavaTest
    public void setNewSuggestions_cachedSuggestionsWithPostdataBeforeAndAfterAreSame() {
        List<OmniboxSuggestion> list1 = buildDummySuggestionsList(2, true);
        OmniboxSuggestion.cacheOmniboxSuggestionListForZeroSuggest(list1);
        List<OmniboxSuggestion> list2 =
                OmniboxSuggestion.getCachedOmniboxSuggestionsForZeroSuggest();
        Assert.assertTrue(isOmniboxSuggestionListMatch(list1, list2));
    }

    @CalledByNativeJavaTest
    public void setNewSuggestions_cachedSuggestionsWithoutPostdataBeforeAndAfterAreSame() {
        List<OmniboxSuggestion> list1 = buildDummySuggestionsList(2, false);
        OmniboxSuggestion.cacheOmniboxSuggestionListForZeroSuggest(list1);
        List<OmniboxSuggestion> list2 =
                OmniboxSuggestion.getCachedOmniboxSuggestionsForZeroSuggest();
        Assert.assertTrue(isOmniboxSuggestionListMatch(list1, list2));
    }
}
