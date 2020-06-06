// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.View.OnClickListener;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Handles displaying the share sheet. The version used depends on several
 * conditions.
 * Android K and below: custom share dialog
 * Android L+: system share sheet
 * #chrome-sharing-hub enabled: custom share sheet
 */
class ShareSheetPropertyModelBuilder {
    @IntDef({ContentType.LINK_PAGE_VISIBLE, ContentType.LINK_PAGE_NOT_VISIBLE, ContentType.TEXT,
            ContentType.IMAGE, ContentType.OTHER_FILE_TYPE})
    @Retention(RetentionPolicy.SOURCE)
    @interface ContentType {
        int LINK_PAGE_VISIBLE = 0;
        int LINK_PAGE_NOT_VISIBLE = 1;
        int TEXT = 2;
        int IMAGE = 3;
        int OTHER_FILE_TYPE = 4;
    }

    private static final int MAX_NUM_APPS = 7;
    private static final String IMAGE_TYPE = "image/";
    // Variations parameter name for the comma-separated list of third-party activity names.
    private static final String PARAM_SHARING_HUB_THIRD_PARTY_APPS = "sharing-hub-third-party-apps";

    static final HashSet<Integer> ALL_CONTENT_TYPES = new HashSet<>(
            Arrays.asList(ContentType.LINK_PAGE_VISIBLE, ContentType.LINK_PAGE_NOT_VISIBLE,
                    ContentType.TEXT, ContentType.IMAGE, ContentType.OTHER_FILE_TYPE));
    private static final ArrayList<String> FALLBACK_ACTIVITIES =
            new ArrayList(Arrays.asList("com.whatsapp.ContactPicker",
                    "com.facebook.composer.shareintent.ImplicitShareIntentHandlerDefaultAlias",
                    "com.google.android.gm.ComposeActivityGmailExternal",
                    "com.instagram.share.handleractivity.StoryShareHandlerActivity",
                    "com.facebook.messenger.intents.ShareIntentHandler",
                    "com.google.android.apps.messaging.ui.conversationlist.ShareIntentActivity",
                    "com.twitter.composer.ComposerActivity", "com.snap.mushroom.MainActivity",
                    "com.pinterest.activity.create.PinItActivity",
                    "com.linkedin.android.publishing.sharing.LinkedInDeepLinkActivity",
                    "jp.naver.line.android.activity.selectchat.SelectChatActivityLaunchActivity",
                    "com.facebook.lite.composer.activities.ShareIntentMultiPhotoAlphabeticalAlias",
                    "com.facebook.mlite.share.view.ShareActivity",
                    "com.samsung.android.email.ui.messageview.MessageFileView",
                    "com.yahoo.mail.ui.activities.ComposeActivity",
                    "org.telegram.ui.LaunchActivity", "com.tencent.mm.ui.tools.ShareImgUI"));

    private final BottomSheetController mBottomSheetController;
    private final PackageManager mPackageManager;

    ShareSheetPropertyModelBuilder(
            BottomSheetController bottomSheetController, PackageManager packageManager) {
        mBottomSheetController = bottomSheetController;
        mPackageManager = packageManager;
    }

    /**
     * Returns a set of {@link ShareSheetCoordinator.ContentType}s for the current share.
     *
     * <p>If {@link ChromeFeatureList.CHROME_SHARING_HUB_V15} is not enabled, this returns a set of
     * all of the {@link ContentType}s. Otherwise, it adds {@link ContentType}s according to the
     * following logic:
     *
     * <ul>
     *     <li>If a URL is present, {@code isUrlOfVisiblePage} determines whether to add
     *     {@link ContentType.LINK_PAGE_VISIBLE} or {@link ContentType.LINK_PAGE_NOT_VISIBLE}.
     *     <li>If the text being shared is not the same as the URL, add {@link ContentType.TEXT}
     *     <li>If the share contains files and the {@code fileContentType} is an image, add
     *     {@link ContentType.IMAGE}. Otherwise, add {@link ContentType.OTHER_FILE_TYPE}.
     * </ul>
     */
    static Set<Integer> getContentTypes(ShareParams params, boolean isUrlOfVisiblePage) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SHARING_HUB_V15)) {
            return ALL_CONTENT_TYPES;
        }
        Set<Integer> contentTypes = new HashSet<>();
        if (!TextUtils.isEmpty(params.getUrl())) {
            if (isUrlOfVisiblePage) {
                contentTypes.add(ContentType.LINK_PAGE_VISIBLE);
            } else {
                contentTypes.add(ContentType.LINK_PAGE_NOT_VISIBLE);
            }
        }
        if (!TextUtils.isEmpty(params.getText())
                && !TextUtils.equals(params.getUrl(), params.getText())) {
            contentTypes.add(ContentType.TEXT);
        }
        if (params.getFileUris() != null) {
            if (!TextUtils.isEmpty(params.getFileContentType())
                    && params.getFileContentType().startsWith(IMAGE_TYPE)) {
                contentTypes.add(ContentType.IMAGE);
            } else {
                contentTypes.add(ContentType.OTHER_FILE_TYPE);
            }
        }
        return contentTypes;
    }

    ArrayList<PropertyModel> selectThirdPartyApps(ShareSheetBottomSheetContent bottomSheet,
            ShareParams params, boolean saveLastUsed, long shareStartTime) {
        List<String> thirdPartyActivityNames = getThirdPartyActivityNames();
        Intent intent = ShareHelper.getShareLinkAppCompatibilityIntent();
        final ShareParams.TargetChosenCallback callback = params.getCallback();
        List<ResolveInfo> resolveInfoList = mPackageManager.queryIntentActivities(intent, 0);
        List<ResolveInfo> thirdPartyActivities = new ArrayList<>();

        // Construct a list of 3P apps. The list should be sorted by the country-specific ranking
        // when available or the fallback list defined above.  If less than MAX_NUM_APPS are
        // available the list is filled with whatever else is available.
        for (String s : thirdPartyActivityNames) {
            for (ResolveInfo res : resolveInfoList) {
                if (res.activityInfo.name.equals(s)) {
                    thirdPartyActivities.add(res);
                    resolveInfoList.remove(res);
                    break;
                }
            }
            if (thirdPartyActivities.size() == MAX_NUM_APPS) {
                break;
            }
        }
        for (ResolveInfo res : resolveInfoList) {
            thirdPartyActivities.add(res);
            if (thirdPartyActivities.size() == MAX_NUM_APPS) {
                break;
            }
        }

        ArrayList<PropertyModel> models = new ArrayList<>();
        for (int i = 0; i < MAX_NUM_APPS && i < thirdPartyActivities.size(); ++i) {
            ResolveInfo info = thirdPartyActivities.get(i);
            final int logIndex = i;
            PropertyModel propertyModel =
                    createPropertyModel(ShareHelper.loadIconForResolveInfo(info, mPackageManager),
                            (String) info.loadLabel(mPackageManager), (shareParams) -> {
                                RecordUserAction.record("SharingHubAndroid.ThirdPartyAppSelected");
                                RecordHistogram.recordEnumeratedHistogram(
                                        "Sharing.SharingHubAndroid.ThirdPartyAppUsage", logIndex,
                                        MAX_NUM_APPS + 1);
                                RecordHistogram.recordMediumTimesHistogram(
                                        "Sharing.SharingHubAndroid.TimeToShare",
                                        System.currentTimeMillis() - shareStartTime);
                                ActivityInfo ai = info.activityInfo;

                                ComponentName component =
                                        new ComponentName(ai.applicationInfo.packageName, ai.name);
                                if (callback != null) {
                                    callback.onTargetChosen(component);
                                }
                                if (saveLastUsed) {
                                    ShareHelper.setLastShareComponentName(component);
                                }
                                mBottomSheetController.hideContent(bottomSheet, true);
                                ShareHelper.shareDirectly(params, component);
                            }, /*isFirstParty=*/false);
            models.add(propertyModel);
        }
        return models;
    }

    static PropertyModel createPropertyModel(
            Drawable icon, String label, OnClickListener listener, boolean isFirstParty) {
        return new PropertyModel.Builder(ShareSheetItemViewProperties.ALL_KEYS)
                .with(ShareSheetItemViewProperties.ICON, icon)
                .with(ShareSheetItemViewProperties.LABEL, label)
                .with(ShareSheetItemViewProperties.CLICK_LISTENER, listener)
                .with(ShareSheetItemViewProperties.IS_FIRST_PARTY, isFirstParty)
                .build();
    }

    private List<String> getThirdPartyActivityNames() {
        String param = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.CHROME_SHARING_HUB, PARAM_SHARING_HUB_THIRD_PARTY_APPS);
        if (param.isEmpty()) {
            return FALLBACK_ACTIVITIES;
        }
        return new ArrayList<String>(Arrays.asList(param.split(",")));
    }
}
