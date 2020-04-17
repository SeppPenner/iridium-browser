// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.image_fetcher;

import android.graphics.Bitmap;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.DiscardableReferencePool;
import org.chromium.chrome.browser.util.BitmapCache;
import org.chromium.components.browser_ui.util.ConversionUtils;

import jp.tomorrowkey.android.gifplayer.BaseGifImage;

/**
 * ImageFetcher implementation with an in-memory cache. Can also be configured to use a disk cache.
 */
public class InMemoryCachedImageFetcher extends ImageFetcher {
    public static final int DEFAULT_CACHE_SIZE = 20 * ConversionUtils.BYTES_PER_MEGABYTE; // 20mb
    private static final float PORTION_OF_AVAILABLE_MEMORY = 1.f / 8.f;

    // Will do the work if the image isn't cached in memory.
    private ImageFetcher mImageFetcher;
    private BitmapCache mBitmapCache;
    private @ImageFetcherConfig int mConfig;

    /**
     * Create an instance with a custom max cache size.
     *
     * @param referencePool Pool used to discard references when under memory pressure.
     * @param cacheSize The cache size to use (in bytes), may be smaller depending on the device's
     *         memory.
     */
    InMemoryCachedImageFetcher(@NonNull ImageFetcher imageFetcher,
            @NonNull DiscardableReferencePool referencePool, int cacheSize) {
        this(imageFetcher,
                new BitmapCache(referencePool,
                        InMemoryCachedImageFetcher.determineCacheSize(
                                (int) Runtime.getRuntime().freeMemory(), cacheSize)));
    }

    /**
     * @param referencePool Pool used to discard references when under memory pressure.
     * @param bitmapCache The cached where bitmaps will be stored in memory.
     *         memory.
     */
    InMemoryCachedImageFetcher(
            @NonNull ImageFetcher imageFetcher, @NonNull BitmapCache bitmapCache) {
        super(imageFetcher);
        mBitmapCache = bitmapCache;
        mImageFetcher = imageFetcher;

        @ImageFetcherConfig
        int underlyingConfig = mImageFetcher.getConfig();
        assert (underlyingConfig == ImageFetcherConfig.NETWORK_ONLY
                || underlyingConfig == ImageFetcherConfig.DISK_CACHE_ONLY)
            : "Invalid underlying config for InMemoryCachedImageFetcher";

        // Determine the config based on the composited image fetcher.
        if (mImageFetcher.getConfig() == ImageFetcherConfig.NETWORK_ONLY) {
            mConfig = ImageFetcherConfig.IN_MEMORY_ONLY;
        } else if (mImageFetcher.getConfig() == ImageFetcherConfig.DISK_CACHE_ONLY) {
            mConfig = ImageFetcherConfig.IN_MEMORY_WITH_DISK_CACHE;
        } else {
            // Report in memory only if none can be found.
            mConfig = ImageFetcherConfig.IN_MEMORY_ONLY;
        }
    }

    @Override
    public void destroy() {
        if (mImageFetcher != null) {
            mImageFetcher.destroy();
            mImageFetcher = null;
        }

        if (mBitmapCache != null) {
            mBitmapCache.destroy();
            mBitmapCache = null;
        }
    }

    @Override
    public void fetchGif(String url, String clientName, Callback<BaseGifImage> callback) {
        assert mBitmapCache != null && mImageFetcher != null : "fetchGif called after destroy";
        mImageFetcher.fetchGif(url, clientName, callback);
    }

    @Override
    public void fetchImage(
            String url, String clientName, int width, int height, Callback<Bitmap> callback) {
        assert mBitmapCache != null && mImageFetcher != null : "fetchImage called after destroy";
        Bitmap cachedBitmap = tryToGetBitmap(url, width, height);
        if (cachedBitmap == null) {
            mImageFetcher.fetchImage(url, clientName, width, height, (@Nullable Bitmap bitmap) -> {
                storeBitmap(bitmap, url, width, height);
                callback.onResult(bitmap);
            });
        } else {
            reportEvent(clientName, ImageFetcherEvent.JAVA_IN_MEMORY_CACHE_HIT);
            callback.onResult(cachedBitmap);
        }
    }

    @Override
    public void clear() {
        assert mBitmapCache != null && mImageFetcher != null : "clear called after destroy";
        mBitmapCache.clear();
    }

    @Override
    public @ImageFetcherConfig int getConfig() {
        return mConfig;
    }

    /**
     * Try to get a bitmap from the in-memory cache. Returns null if this object has been destroyed.
     *
     * @param url The url of the image.
     * @param width The width (in pixels) of the image.
     * @param height The height (in pixels) of the image.
     * @return The Bitmap stored in memory or null.
     */
    @VisibleForTesting
    Bitmap tryToGetBitmap(String url, int width, int height) {
        if (mBitmapCache == null) return null;

        String key = encodeCacheKey(url, width, height);
        return mBitmapCache.getBitmap(key);
    }

    /**
     * Store the bitmap in memory.
     *
     * @param url The url of the image.
     * @param width The width (in pixels) of the image.
     * @param height The height (in pixels) of the image.
     */
    private void storeBitmap(@Nullable Bitmap bitmap, String url, int width, int height) {
        if (bitmap == null || mBitmapCache == null) {
            return;
        }

        String key = encodeCacheKey(url, width, height);
        mBitmapCache.putBitmap(key, bitmap);
    }

    /**
     * Use the given parameters to encode a key used in the String -> Bitmap mapping.
     *
     * @param url The url of the image.
     * @param width The width (in pixels) of the image.
     * @param height The height (in pixels) of the image.
     * @return The key for the BitmapCache.
     */
    @VisibleForTesting
    String encodeCacheKey(String url, int width, int height) {
        // Encoding for cache key is:
        // <url>/<width>/<height>.
        return url + "/" + width + "/" + height;
    }

    /**
     * Size the cache size depending on available memory and the client's preferred cache size.
     *
     * The size of the cache in memory cache to 18th of the available system memory or the
     * requested cache size, which ever is smaller.
     *
     * @param runtime The Java runtime, used to determine the available memory on th edevice.
     * @param preferredCacheSize The preferred cache size (in bytes).
     * @return The actual size of the cache (in bytes).
     */
    @VisibleForTesting
    static int determineCacheSize(int freeMemory, int preferredCacheSize) {
        // Try to size the cache according to client's wishes. If there's not enough space, then
        // take a portion of available memory.
        final int maxCacheUsage = (int) (freeMemory * PORTION_OF_AVAILABLE_MEMORY);
        return Math.min(maxCacheUsage, preferredCacheSize);
    }
}
