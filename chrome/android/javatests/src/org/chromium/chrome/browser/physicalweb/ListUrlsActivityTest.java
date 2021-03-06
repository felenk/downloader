// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.physicalweb;

import android.app.Instrumentation.ActivityMonitor;
import android.content.ComponentName;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.View;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.privacy.PrivacyPreferencesManager;
import org.chromium.chrome.test.util.browser.notifications.MockNotificationManagerProxy;
import org.chromium.content.browser.test.util.TestTouchUtils;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for the ListUrlsActivity class.
 */
public class ListUrlsActivityTest extends InstrumentationTestCase {
    static final String URL = "https://example.com/";
    static final String TITLE = "Example Title";
    static final String DESC = "Example Website";

    Context mContext;
    MockPwsClient mMockPwsClient;

    private static class TestContextWrapper extends ContextWrapper {
        public List<Intent> startedIntents;

        TestContextWrapper(Context context) {
            super(context);
            startedIntents = new ArrayList<>();
        }

        @Override
        public void startActivity(Intent intent) {
            startedIntents.add(intent);
            super.startActivity(intent);
        }
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mContext = getInstrumentation().getTargetContext();
        UrlManager urlManager = UrlManager.getInstance(mContext);
        mMockPwsClient = new MockPwsClient();
        urlManager.overridePwsClientForTesting(mMockPwsClient);
        urlManager.overrideNotificationManagerForTesting(new MockNotificationManagerProxy());
    }

    @SmallTest
    public void testTapEntryOpensUrl() {
        // Ensure the Physical Web is enabled.
        PrivacyPreferencesManager prefsManager = PrivacyPreferencesManager.getInstance(mContext);
        prefsManager.setPhysicalWebEnabled(true);
        assertFalse(prefsManager.isPhysicalWebOnboarding());
        assertTrue(prefsManager.isPhysicalWebEnabled());

        // Add URL.
        addUrl(URL, TITLE, DESC);

        // Launch the Activity.
        ListUrlsActivity listActivity = launchActivity();
        TestContextWrapper testContextWrapper = new TestContextWrapper(listActivity);
        listActivity.overrideContextForTesting(testContextWrapper);
        getInstrumentation().waitForIdleSync();
        View listView = listActivity.findViewById(R.id.physical_web_urls_list);

        // Read the activity and tap the list entry.
        ArrayList<View> entries = new ArrayList<>();
        listView.findViewsWithText(entries, TITLE, View.FIND_VIEWS_WITH_TEXT);
        assertEquals(1, entries.size());
        View entry = entries.get(0);
        TestTouchUtils.clickView(this, entry);

        // Test the fired intent.
        assertEquals(1, testContextWrapper.startedIntents.size());
        assertEquals(URL, testContextWrapper.startedIntents.get(0).getDataString());
    }

    // Onboarding tests are broken (see bug 608872)
    @SmallTest
    @DisabledTest
    public void testUrlsListEmptyInOnboarding() {
        // In onboarding, we scan for nearby URLs but do not send them to the resolution service to
        // protect the user's privacy. This test checks that the URL list, which only displays
        // resolved URLs, is empty during onboarding.
        PrivacyPreferencesManager prefsManager = PrivacyPreferencesManager.getInstance(mContext);
        assertTrue(prefsManager.isPhysicalWebOnboarding());
        assertFalse(prefsManager.isPhysicalWebEnabled());

        // Add URL.
        addUrl(URL, TITLE, DESC);

        // Launch the Activity.
        ListUrlsActivity listActivity = launchActivity();
        TestContextWrapper testContextWrapper = new TestContextWrapper(listActivity);
        listActivity.overrideContextForTesting(testContextWrapper);
        getInstrumentation().waitForIdleSync();
        View listView = listActivity.findViewById(R.id.physical_web_urls_list);

        // Read the activity and check that there are no matching entries.
        ArrayList<View> entries = new ArrayList<>();
        listView.findViewsWithText(entries, TITLE, View.FIND_VIEWS_WITH_TEXT);
        assertEquals(0, entries.size());
    }

    @SmallTest
    public void testUrlsListEmptyWithPhysicalWebDisabled() {
        // With the Physical Web disabled, the URLs list should always be empty.
        PrivacyPreferencesManager prefsManager = PrivacyPreferencesManager.getInstance(mContext);
        prefsManager.setPhysicalWebEnabled(false);
        assertFalse(prefsManager.isPhysicalWebOnboarding());
        assertFalse(prefsManager.isPhysicalWebEnabled());

        // Add URL.
        addUrl(URL, TITLE, DESC);

        // Launch the Activity.
        ListUrlsActivity listActivity = launchActivity();
        TestContextWrapper testContextWrapper = new TestContextWrapper(listActivity);
        listActivity.overrideContextForTesting(testContextWrapper);
        getInstrumentation().waitForIdleSync();
        View listView = listActivity.findViewById(R.id.physical_web_urls_list);

        // Read the activity and check that there are no matching entries.
        ArrayList<View> entries = new ArrayList<>();
        listView.findViewsWithText(entries, TITLE, View.FIND_VIEWS_WITH_TEXT);
        assertEquals(0, entries.size());
    }

    public void addUrl(String url, String title, String desc) {
        ArrayList<PwsResult> results = new ArrayList<>();
        results.add(new PwsResult(url, url, null, title, desc));
        mMockPwsClient.addPwsResults(results);
        mMockPwsClient.addPwsResults(results);
        UrlManager.getInstance(mContext).addUrl(url);
        getInstrumentation().waitForIdleSync();
    }

    public ListUrlsActivity launchActivity() {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setComponent(new ComponentName(mContext, ListUrlsActivity.class));
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        ActivityMonitor monitor =
                getInstrumentation().addMonitor(ListUrlsActivity.class.getName(), null, false);
        mContext.startActivity(intent);
        ListUrlsActivity activity = (ListUrlsActivity) monitor.waitForActivity();
        activity.overridePwsClientForTesting(mMockPwsClient);
        return activity;
    }
}
