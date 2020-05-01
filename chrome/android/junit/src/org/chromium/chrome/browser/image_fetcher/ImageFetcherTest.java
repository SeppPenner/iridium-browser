// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.image_fetcher;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.graphics.Bitmap;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;

import jp.tomorrowkey.android.gifplayer.BaseGifImage;

/**
 * Test for ImageFetcher.java.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ImageFetcherTest {
    private static final int WIDTH_PX = 100;
    private static final int HEIGHT_PX = 200;
    private static final int EXPIRATION_INTERVAL = 60;

    /**
     * Concrete implementation for testing purposes.
     */
    private class ImageFetcherForTest extends ImageFetcher {
        ImageFetcherForTest(ImageFetcherBridge imageFetcherBridge) {
            super(imageFetcherBridge);
        }

        @Override
        public void fetchGif(String url, String clientName, Callback<BaseGifImage> callback) {}

        @Override
        public void fetchImage(
                String url, String clientName, int width, int height, Callback<Bitmap> callback) {}

        @Override
        public void clear() {}

        @Override
        public int getConfig() {
            return 0;
        }

        @Override
        public void destroy() {}
    }

    @Mock
    ImageFetcherBridge mBridge;

    private final Bitmap mBitmap =
            Bitmap.createBitmap(WIDTH_PX, HEIGHT_PX, Bitmap.Config.ARGB_8888);

    private ImageFetcherForTest mImageFetcher;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mImageFetcher = Mockito.spy(new ImageFetcherForTest(mBridge));
    }

    @Test
    public void testResize() {
        Bitmap result = ImageFetcher.resizeImage(mBitmap, WIDTH_PX / 2, HEIGHT_PX / 2);
        assertNotEquals(result, mBitmap);
    }

    @Test
    public void testResizeBailsOutIfSizeIsZeroOrLess() {
        Bitmap result = ImageFetcher.resizeImage(mBitmap, WIDTH_PX - 1, HEIGHT_PX - 1);
        assertNotEquals(result, mBitmap);

        result = ImageFetcher.resizeImage(mBitmap, 0, HEIGHT_PX);
        assertEquals(result, mBitmap);

        result = ImageFetcher.resizeImage(mBitmap, WIDTH_PX, 0);
        assertEquals(result, mBitmap);

        result = ImageFetcher.resizeImage(mBitmap, 0, 0);
        assertEquals(result, mBitmap);

        result = ImageFetcher.resizeImage(mBitmap, -1, HEIGHT_PX);
        assertEquals(result, mBitmap);

        result = ImageFetcher.resizeImage(mBitmap, WIDTH_PX, -1);
        assertEquals(result, mBitmap);
    }

    @Test
    public void testFetchImageNoDimensionsAlias() {
        String url = "url";
        String client = "client";
        mImageFetcher.fetchImage(url, client, result -> {});

        // No arguments should alias to 0, 0.
        verify(mImageFetcher).fetchImage(eq(url), eq(client), eq(0), eq(0), any());
    }

    @Test
    public void testCreateParams() {
        String url = "https://www.example.com/image";
        String client = "client";

        // Verifies params without size specified.
        ImageFetcher.Params params = ImageFetcher.Params.create(url, client);
        assertEquals(url, params.url);
        assertEquals(client, params.clientName);
        assertEquals(0, params.width);
        assertEquals(0, params.height);
        assertEquals(0, params.expirationInterval);

        // Verifies params with size.
        params = ImageFetcher.Params.create(url, client, WIDTH_PX, HEIGHT_PX);
        assertEquals(url, params.url);
        assertEquals(client, params.clientName);
        assertEquals(WIDTH_PX, params.width);
        assertEquals(HEIGHT_PX, params.height);
        assertEquals(0, params.expirationInterval);
    }

    @Test
    public void testCreateParamsWithExpirationInterval() {
        String url = "https://www.example.com/image";
        String client = "client";

        // Verifies params with expiration interval.
        ImageFetcher.Params params = ImageFetcher.Params.createWithExpirationInterval(
                url, client, WIDTH_PX, HEIGHT_PX, EXPIRATION_INTERVAL);
        assertEquals(url, params.url);
        assertEquals(client, params.clientName);
        assertEquals(WIDTH_PX, params.width);
        assertEquals(HEIGHT_PX, params.height);
        assertEquals(EXPIRATION_INTERVAL, params.expirationInterval);
    }
}
