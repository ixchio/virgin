#include "FilterUpdater.hpp"
#include "FilterCompiler.hpp"
#include "AdBlockEngine.hpp"
#include "app/Paths.hpp"
#include "app/Version.hpp"

#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QThread>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QSaveFile>
#include <QFileInfo>
#include <QDateTime>

namespace virgin::adblock {

FilterUpdater::FilterUpdater(AdBlockEngine* engine, QObject* parent)
    : QObject(parent), engine_(engine), network_(new QNetworkAccessManager(this)) {}

FilterUpdater::~FilterUpdater() = default;

void FilterUpdater::setSources(const QList<QUrl>& sources) { sources_ = sources; }

bool FilterUpdater::needsNetworkUpdate(int maxAgeDays) const {
    const QDir directory(app::Paths::filtersDir());
    const QFileInfoList subscriptions = directory.entryInfoList(
        {QStringLiteral("subscription-*.txt")}, QDir::Files, QDir::Name);
    if (subscriptions.size() < 2) return true;
    const QDateTime cutoff = QDateTime::currentDateTimeUtc().addDays(-qMax(1, maxAgeDays));
    for (const QFileInfo& file : subscriptions) {
        if (file.size() <= 0 || file.lastModified().toUTC() < cutoff) return true;
    }
    return false;
}

void FilterUpdater::updateNow() {
    if (updating_.exchange(true)) {
        emit updateFinished(false, "Update already in progress");
        return;
    }
    emit updateStarted();

    // Collect local paths on main thread (fast)
    QStringList localPaths = {
        app::Paths::filtersDir() + "/easylist.txt",
        app::Paths::filtersDir() + "/easyprivacy.txt"
    };
    QStringList existing;
    for (auto& p : localPaths) if (QFile::exists(p)) existing << p;
    if (existing.isEmpty()) {
        existing << app::Paths::builtinFiltersDir() + "/easylist.txt";
        existing << app::Paths::builtinFiltersDir() + "/easyprivacy.txt";
    }
    const QString maintainedRules = app::Paths::builtinFiltersDir() +
        QStringLiteral("/virgin-protection.txt");
    if (QFile::exists(maintainedRules)) existing << maintainedRules;
    compilePaths(existing);
}

void FilterUpdater::compilePaths(const QStringList& paths) {
    emit updateProgress(60);

    // Worker compilation keeps parsing and index construction off the UI thread.
    const QPointer<FilterUpdater> self(this);
    QThread* worker = QThread::create([self, paths]{
        // Background thread: heavy compilation, no UI, no I/O on hot path
        // Sec 41: validate size, syntax, compile safely, preserve previous on failure
        auto compiled = FilterCompiler::compileFiles(paths);

        // Validation (Sec 41): size limit, rule length, regex complexity
        bool ok = compiled && (compiled->blockRules.size() + compiled->allowRules.size() > 0);
        QString msg;
        if (!ok) msg = "No rules compiled";
        else if (compiled->blockRules.size() + compiled->allowRules.size() > 500000) {
            ok = false;
            msg = "Rule count exceeds limit (500k)";
        } else {
            msg = QString("Compiled %1 block + %2 allow (%3 suffix, %4 aho nodes)")
                      .arg(compiled->blockRules.size()).arg(compiled->allowRules.size())
                      .arg(compiled->blockSuffixTrie.size() + compiled->allowSuffixTrie.size())
                      .arg(compiled->blockAho.nodeCount() + compiled->allowAho.nodeCount());
        }

        // Back to main thread for atomic swap and cache write (queued)
        if (!self) return;
        QMetaObject::invokeMethod(self, [self, compiled, ok, msg]{
            FilterUpdater* updater = self.data();
            if (!updater || !updater->engine_) return;
            if (ok && compiled) {
                // Sec 44: atomic shared_ptr swap — zero downtime, no tab reload
                updater->engine_->updateRules(compiled);

                // Write binary cache atomically (Sec 41)
                QString cacheDir = app::Paths::compiledFiltersDir();
                QDir().mkpath(cacheDir);
                QString final = cacheDir + "/rules.bin";
                FilterCompiler::writeCache(*compiled, final);
                emit updater->updateProgress(100);
                emit updater->updateFinished(true, msg);
            } else {
                // Preserve previous valid list
                emit updater->updateFinished(false, msg);
            }
            updater->updating_.store(false);
        }, Qt::QueuedConnection);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void FilterUpdater::updateFromNetwork() {
    if (updating_.exchange(true)) {
        emit updateFinished(false, "Update already in progress");
        return;
    }
    if (sources_.isEmpty()) {
        sources_ = {
            QUrl(QStringLiteral("https://easylist.to/easylist/easylist.txt")),
            QUrl(QStringLiteral("https://easylist.to/easylist/easyprivacy.txt"))
        };
    }
    for (const QUrl& source : sources_) {
        if (!source.isValid() || source.scheme().toLower() != "https") {
            finishNetworkFailure("Every filter source must use HTTPS");
            return;
        }
    }
    emit updateStarted();
    emit updateProgress(5);
    downloadedLists_.clear();
    sourceIndex_ = 0;
    fetchNext();
}

void FilterUpdater::fetchNext() {
    if (sourceIndex_ >= sources_.size()) {
        QDir().mkpath(app::Paths::filtersDir());
        QStringList paths;
        for (qsizetype index = 0; index < downloadedLists_.size(); ++index) {
            const QString path = app::Paths::filtersDir() +
                QStringLiteral("/subscription-%1.txt").arg(index);
            QSaveFile file(path);
            if (!file.open(QIODevice::WriteOnly) ||
                file.write(downloadedLists_[index]) != downloadedLists_[index].size() ||
                !file.commit()) {
                finishNetworkFailure("Could not save downloaded filter lists");
                return;
            }
            paths.append(path);
        }
        const QString maintainedRules = app::Paths::builtinFiltersDir() +
            QStringLiteral("/virgin-protection.txt");
        if (QFile::exists(maintainedRules)) paths.append(maintainedRules);
        compilePaths(paths);
        return;
    }

    QNetworkRequest request(sources_[sourceIndex_]);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(30'000);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Virgin/%1 filter updater").arg(app::kVersion));
    QNetworkReply* reply = network_->get(request);
    auto* payload = new QByteArray();
    payload->reserve(1024 * 1024);
    connect(reply, &QIODevice::readyRead, this, [reply, payload] {
        payload->append(reply->readAll());
        if (payload->size() > kMaxListSize) reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, payload] {
        const QByteArray body = std::move(*payload);
        delete payload;
        const bool tooLarge = body.size() > kMaxListSize;
        const bool secureFinalUrl = reply->url().scheme().toLower() == "https";
        const QString error = reply->errorString();
        const bool success = reply->error() == QNetworkReply::NoError &&
                             !body.isEmpty() && !tooLarge && secureFinalUrl;
        reply->deleteLater();
        if (!success) {
            finishNetworkFailure(tooLarge ? "Filter list exceeds 10 MiB limit"
                                          : "Filter download failed: " + error);
            return;
        }
        downloadedLists_.append(body);
        ++sourceIndex_;
        emit updateProgress(static_cast<int>(5 + sourceIndex_ * 45 / sources_.size()));
        fetchNext();
    });
}

void FilterUpdater::finishNetworkFailure(const QString& message) {
    downloadedLists_.clear();
    sourceIndex_ = 0;
    updating_.store(false);
    emit updateFinished(false, message);
}

} // namespace virgin::adblock
