// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifdef CHIAKI_IS_MAC_APPSTORE

#include "appreviewmanager.h"

#include <QGuiApplication>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(chiakiAppReview, "chiaki.appreview")

// Defined in appreviewbridge.mm — keeps StoreKit confined to one Obj-C++ TU.
extern void PyluxAppReview_RequestReview();

namespace {
constexpr qint64 kMinFirstStreamMs = 10LL * 60LL * 1000LL;          // 10 min
constexpr qint64 kBetweenPromptsStreamMs = 60LL * 60LL * 1000LL;    // 60 min
}

AppReviewManager::AppReviewManager(Settings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
}

void AppReviewManager::armOnNextActivation()
{
    if (m_armed)
        return;
    m_armed = true;

    auto *app = qGuiApp;
    if (!app)
        return;

    // Already active: defer one event-loop tick. Otherwise listen for the next active transition.
    if (app->applicationState() == Qt::ApplicationActive) {
        QMetaObject::invokeMethod(this, [this] { requestReviewIfEligible(); }, Qt::QueuedConnection);
        return;
    }

    m_activationConn = connect(app, &QGuiApplication::applicationStateChanged, this,
        [this](Qt::ApplicationState state) {
            if (state != Qt::ApplicationActive)
                return;
            QObject::disconnect(m_activationConn);
            requestReviewIfEligible();
        });
}

void AppReviewManager::requestReviewIfEligible()
{
    if (m_requestedThisLaunch || !m_settings)
        return;

    const qint64 total = m_settings->GetDonationTotalStreamTimeMs();
    const qint64 last = m_settings->GetAppReviewLastPromptTotalStreamMs();
    const qint64 needed = (last == 0) ? kMinFirstStreamMs : (last + kBetweenPromptsStreamMs);
    qCInfo(chiakiAppReview).nospace()
        << "App review: eligibility check (total=" << total
        << " last=" << last
        << " needed=" << needed << ")";
    if (total < needed)
        return;

    m_requestedThisLaunch = true;
    qCInfo(chiakiAppReview) << "App review: requested (system may not display)";

    PyluxAppReview_RequestReview();

    // Persist *after* the bridge call; +60 min throttle still bounds re-prompts if it threw.
    m_settings->SetAppReviewLastPromptTotalStreamMs(total);
}

#endif // CHIAKI_IS_MAC_APPSTORE
