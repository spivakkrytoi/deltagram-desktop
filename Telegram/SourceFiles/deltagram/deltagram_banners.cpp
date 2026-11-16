/*
This file is part of DeltaGram Desktop,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "deltagram/deltagram_banners.h"

#include "data/data_peer.h"
#include "data/data_peer_id.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QHash>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtGui/QColor>
#include <QtGui/QPainter>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QtWidgets/QApplication>

#include <algorithm>
#include <cmath>

namespace Deltagram::Banners {
namespace {

constexpr auto kBaseUrl = "https://dltagram.duckdns.org";
constexpr auto kRegistryUrl = "https://dltagram.duckdns.org/api/banners/registry";
constexpr auto kRefreshInterval = 10 * 60 * 1000; // 10 minutes
constexpr auto kTransferTimeout = 15000;
constexpr auto kUserAgent = "DeltaGramDesktop";

class Registry final : public QObject {
public:
	static Registry &Instance();

	[[nodiscard]] QImage For(BareId userId);
	void EnsureStarted();

private:
	Registry() = default;

	void Refresh();
	void Apply(const QByteArray &data);
	void Load(BareId userId, const QString &token);
	void RepaintEverything();

	QNetworkAccessManager _manager;
	QTimer _timer;
	QHash<BareId, QString> _tokens;
	QHash<BareId, QImage> _images;
	QSet<BareId> _loading;
	bool _started = false;
	bool _inFlight = false;

};

Registry &Registry::Instance() {
	// Intentionally leaked: it owns a QNetworkAccessManager and must outlive
	// widgets that paint through it during shutdown.
	static auto result = new Registry();
	return *result;
}

void Registry::EnsureStarted() {
	if (_started) {
		return;
	}
	const auto app = QCoreApplication::instance();
	if (!app || QThread::currentThread() != app->thread()) {
		return;
	}
	_started = true;
	_timer.setInterval(kRefreshInterval);
	QObject::connect(&_timer, &QTimer::timeout, [=] {
		Refresh();
	});
	_timer.start();
	Refresh();
}

void Registry::Refresh() {
	if (_inFlight) {
		return;
	}
	_inFlight = true;

	auto request = QNetworkRequest(QUrl(QString::fromLatin1(kRegistryUrl)));
	request.setAttribute(
		QNetworkRequest::RedirectPolicyAttribute,
		QNetworkRequest::NoLessSafeRedirectPolicy);
	request.setHeader(
		QNetworkRequest::UserAgentHeader,
		QString::fromLatin1(kUserAgent));
	request.setTransferTimeout(kTransferTimeout);

	const auto reply = _manager.get(request);
	QObject::connect(reply, &QNetworkReply::finished, this, [=] {
		_inFlight = false;
		if (reply->error() == QNetworkReply::NoError) {
			Apply(reply->readAll());
		}
		reply->deleteLater();
	});
}

void Registry::Apply(const QByteArray &data) {
	const auto document = QJsonDocument::fromJson(data);
	if (!document.isObject()) {
		return;
	}
	const auto list = document.object().value(QStringLiteral("banners")).toObject();
	auto next = QHash<BareId, QString>();
	for (auto i = list.begin(); i != list.end(); ++i) {
		auto ok = false;
		const auto userId = i.key().toULongLong(&ok);
		const auto token = i.value().toString();
		if (ok && !token.isEmpty()) {
			next.insert(userId, token);
		}
	}

	// Drop cached images for accounts that removed their banner, and load the
	// ones whose version token changed.
	for (auto i = _images.begin(); i != _images.end();) {
		if (!next.contains(i.key()) || _tokens.value(i.key()) != next.value(i.key())) {
			i = _images.erase(i);
		} else {
			++i;
		}
	}
	_tokens = std::move(next);
	for (auto i = _tokens.begin(); i != _tokens.end(); ++i) {
		if (!_images.contains(i.key())) {
			Load(i.key(), i.value());
		}
	}
	RepaintEverything();
}

void Registry::Load(BareId userId, const QString &token) {
	if (_loading.contains(userId)) {
		return;
	}
	_loading.insert(userId);

	auto request = QNetworkRequest(QUrl(
		QString::fromLatin1(kBaseUrl)
		+ QStringLiteral("/api/banners/")
		+ QString::number(userId)
		+ QStringLiteral("?v=")
		+ token));
	request.setAttribute(
		QNetworkRequest::RedirectPolicyAttribute,
		QNetworkRequest::NoLessSafeRedirectPolicy);
	request.setHeader(
		QNetworkRequest::UserAgentHeader,
		QString::fromLatin1(kUserAgent));
	request.setTransferTimeout(kTransferTimeout);

	const auto reply = _manager.get(request);
	QObject::connect(reply, &QNetworkReply::finished, this, [=] {
		_loading.remove(userId);
		if (reply->error() == QNetworkReply::NoError) {
			auto image = QImage::fromData(reply->readAll());
			if (!image.isNull() && _tokens.value(userId) == token) {
				_images.insert(userId, std::move(image));
				RepaintEverything();
			}
		}
		reply->deleteLater();
	});
}

void Registry::RepaintEverything() {
	const auto widgets = QApplication::topLevelWidgets();
	for (const auto widget : widgets) {
		if (widget) {
			widget->update();
		}
	}
}

QImage Registry::For(BareId userId) {
	return _images.value(userId);
}

} // namespace

QImage ForPeer(const PeerData *peer) {
	if (!peer || !peer->isUser() || peer->id.value == 0) {
		return QImage();
	}
	auto &registry = Registry::Instance();
	registry.EnsureStarted();
	return registry.For(peerToUser(peer->id).bare);
}

void Paint(QPainter &p, const QRect &rect, const QImage &image) {
	if (image.isNull() || rect.isEmpty()) {
		return;
	}
	const auto scale = std::max(
		rect.width() / double(image.width()),
		rect.height() / double(image.height()));
	const auto sourceWidth = std::min(
		image.width(),
		int(std::round(rect.width() / scale)));
	const auto sourceHeight = std::min(
		image.height(),
		int(std::round(rect.height() / scale)));
	const auto source = QRect(
		(image.width() - sourceWidth) / 2,
		(image.height() - sourceHeight) / 2,
		sourceWidth,
		sourceHeight);
	p.drawImage(rect, image, source);
	p.fillRect(rect, QColor(0, 0, 0, 64));
}

} // namespace Deltagram::Banners
