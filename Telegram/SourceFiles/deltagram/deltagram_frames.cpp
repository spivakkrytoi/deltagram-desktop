/*
This file is part of DeltaGram Desktop,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "deltagram/deltagram_frames.h"

#include "data/data_peer.h"
#include "data/data_peer_id.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QHash>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtGui/QColor>
#include <QtGui/QConicalGradient>
#include <QtGui/QLinearGradient>
#include <QtGui/QPainterPath>
#include <QtGui/QPen>
#include <QtWidgets/QApplication>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <algorithm>
#include <cmath>

namespace Deltagram::Frames {
namespace {

// Public, anonymous registry served by Deltament (see
// cmd/deltament/internal/httpapi/handlers.go).
constexpr auto kRegistryUrl = "https://dltagram.duckdns.org/api/frames/registry";
constexpr auto kRefreshInterval = 10 * 60 * 1000; // 10 minutes
constexpr auto kTransferTimeout = 15000;
constexpr auto kUserAgent = "DeltaGramDesktop";
constexpr auto kPi = 3.14159265358979323846;

[[nodiscard]] Kind ParseKind(const QString &value) {
	const auto id = value.trimmed().toLower();
	if (id == QStringLiteral("scratch")) {
		return Kind::Scratch;
	} else if (id == QStringLiteral("ember")) {
		return Kind::Ember;
	} else if (id == QStringLiteral("neon")) {
		return Kind::Neon;
	} else if (id == QStringLiteral("gold")) {
		return Kind::Gold;
	} else if (id == QStringLiteral("sakura")) {
		return Kind::Sakura;
	} else if (id == QStringLiteral("void")) {
		return Kind::Void;
	} else if (id == QStringLiteral("frost")) {
		return Kind::Frost;
	} else if (id == QStringLiteral("rainbow")) {
		return Kind::Rainbow;
	}
	return Kind::None;
}

class Registry final : public QObject {
public:
	static Registry &Instance();

	[[nodiscard]] Kind For(BareId userId) const;
	void EnsureStarted();

private:
	Registry() = default;

	void Refresh();
	void Apply(const QByteArray &data);
	void RepaintEverything();

	QNetworkAccessManager _manager;
	QTimer _timer;
	QHash<BareId, Kind> _frames;
	bool _started = false;
	bool _inFlight = false;

};

Registry &Registry::Instance() {
	// Intentionally leaked: the registry owns a QNetworkAccessManager and must
	// outlive the widgets that paint through it during shutdown.
	static auto result = new Registry();
	return *result;
}

void Registry::EnsureStarted() {
	if (_started) {
		return;
	}
	const auto app = QCoreApplication::instance();
	if (!app || QThread::currentThread() != app->thread()) {
		// Painting off the main thread must never touch the network stack;
		// the main thread will start the registry on the next repaint.
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
	const auto frames = document.object().value(QStringLiteral("frames")).toObject();
	auto next = QHash<BareId, Kind>();
	for (auto i = frames.begin(); i != frames.end(); ++i) {
		auto ok = false;
		const auto userId = i.key().toULongLong(&ok);
		const auto kind = ParseKind(i.value().toString());
		if (ok && kind != Kind::None) {
			next.insert(userId, kind);
		}
	}
	if (next != _frames) {
		_frames = std::move(next);
		RepaintEverything();
	}
}

void Registry::RepaintEverything() {
	const auto widgets = QApplication::topLevelWidgets();
	for (const auto widget : widgets) {
		if (widget) {
			widget->update();
		}
	}
}

Kind Registry::For(BareId userId) const {
	return _frames.value(userId, Kind::None);
}

[[nodiscard]] QPointF Center(const QRect &rect) {
	return QPointF(
		rect.x() + rect.width() / 2.,
		rect.y() + rect.height() / 2.);
}

void PaintGlow(
		QPainter &p,
		QPointF center,
		qreal radius,
		qreal width,
		const QColor &color,
		qreal alpha = 0.35) {
	auto glow = color;
	glow.setAlphaF(alpha);
	auto pen = QPen(glow, width * 2.4);
	pen.setCapStyle(Qt::RoundCap);
	p.setPen(pen);
	p.setBrush(Qt::NoBrush);
	p.drawEllipse(center, radius, radius);
}

void PaintScratch(QPainter &p, const QRect &rect) {
	const auto size = rect.width();
	const auto center = Center(rect);
	const auto radius = size / 2. - std::max(1., size * 0.012);
	const auto width = std::max(1.6, size * 0.055);

	auto base = QColor(58, 62, 68);
	auto pen = QPen(base, width);
	pen.setCapStyle(Qt::RoundCap);
	p.setPen(pen);
	p.setBrush(Qt::NoBrush);
	p.drawEllipse(center, radius, radius);

	auto path = QPainterPath();
	path.setFillRule(Qt::OddEvenFill);
	path.addEllipse(center, radius + width / 2., radius + width / 2.);
	path.addEllipse(center, radius - width / 2., radius - width / 2.);
	p.save();
	p.setClipPath(path);
	auto scratch = QPen(QColor(242, 244, 248, 150), std::max(0.8, width * 0.22));
	scratch.setCapStyle(Qt::RoundCap);
	p.setPen(scratch);
	for (auto i = 0; i != 9; ++i) {
		const auto angle = i * 37. + 11.;
		const auto radians = angle * kPi / 180.;
		const auto dx = std::cos(radians);
		const auto dy = std::sin(radians);
		const auto length = radius * (0.9 + 0.1 * (i % 3));
		p.drawLine(
			center - QPointF(dx * length, dy * length),
			center + QPointF(dx * length, dy * length));
	}
	p.restore();
}

void PaintEmber(QPainter &p, const QRect &rect) {
	const auto size = rect.width();
	const auto center = Center(rect);
	const auto radius = size / 2. - std::max(1., size * 0.012);
	const auto width = std::max(1.8, size * 0.06);

	auto gradient = QConicalGradient(center, 90.);
	gradient.setColorAt(0.00, QColor(255, 214, 120));
	gradient.setColorAt(0.30, QColor(255, 122, 44));
	gradient.setColorAt(0.60, QColor(214, 48, 32));
	gradient.setColorAt(0.85, QColor(120, 24, 30));
	gradient.setColorAt(1.00, QColor(255, 214, 120));

	PaintGlow(p, center, radius, width, QColor(255, 110, 40), 0.30);

	auto pen = QPen(QBrush(gradient), width);
	pen.setCapStyle(Qt::RoundCap);
	p.setPen(pen);
	p.setBrush(Qt::NoBrush);
	p.drawEllipse(center, radius, radius);

	auto inner = QPen(QColor(255, 236, 200, 120), std::max(0.8, width * 0.16));
	p.setPen(inner);
	p.drawEllipse(center, radius - width * 0.55, radius - width * 0.55);
}

void PaintNeon(QPainter &p, const QRect &rect) {
	const auto size = rect.width();
	const auto center = Center(rect);
	const auto radius = size / 2. - std::max(1., size * 0.014);
	const auto width = std::max(1.6, size * 0.045);

	auto gradient = QConicalGradient(center, 0.);
	gradient.setColorAt(0.00, QColor(64, 255, 240));
	gradient.setColorAt(0.45, QColor(80, 130, 255));
	gradient.setColorAt(0.75, QColor(255, 78, 214));
	gradient.setColorAt(1.00, QColor(64, 255, 240));

	PaintGlow(p, center, radius, width, QColor(90, 220, 255), 0.28);

	auto pen = QPen(QBrush(gradient), width);
	pen.setCapStyle(Qt::RoundCap);
	p.setPen(pen);
	p.setBrush(Qt::NoBrush);
	p.drawEllipse(center, radius, radius);

	auto inner = QPen(QColor(255, 255, 255, 150), std::max(0.7, width * 0.14));
	p.setPen(inner);
	p.drawEllipse(center, radius - width * 0.8, radius - width * 0.8);
}

void PaintGold(QPainter &p, const QRect &rect) {
	const auto size = rect.width();
	const auto center = Center(rect);
	const auto radius = size / 2. - std::max(1., size * 0.012);
	const auto width = std::max(1.8, size * 0.058);

	auto gradient = QLinearGradient(
		rect.topLeft(),
		rect.bottomRight());
	gradient.setColorAt(0.00, QColor(255, 249, 205));
	gradient.setColorAt(0.30, QColor(255, 214, 94));
	gradient.setColorAt(0.55, QColor(196, 140, 32));
	gradient.setColorAt(0.80, QColor(255, 226, 132));
	gradient.setColorAt(1.00, QColor(150, 102, 20));

	PaintGlow(p, center, radius, width, QColor(255, 206, 84), 0.26);

	auto pen = QPen(QBrush(gradient), width);
	pen.setCapStyle(Qt::RoundCap);
	p.setPen(pen);
	p.setBrush(Qt::NoBrush);
	p.drawEllipse(center, radius, radius);

	auto arc = QPen(QColor(255, 255, 238, 190), std::max(0.9, width * 0.20));
	arc.setCapStyle(Qt::RoundCap);
	p.setPen(arc);
	p.drawArc(
		QRectF(
			center.x() - radius,
			center.y() - radius,
			radius * 2.,
			radius * 2.),
		35 * 16,
		70 * 16);
}

void PaintSakura(QPainter &p, const QRect &rect) {
	const auto size = rect.width();
	const auto center = Center(rect);
	const auto radius = size / 2. - std::max(1., size * 0.03);
	const auto width = std::max(1.4, size * 0.038);

	auto gradient = QConicalGradient(center, 45.);
	gradient.setColorAt(0.00, QColor(255, 214, 230));
	gradient.setColorAt(0.50, QColor(255, 132, 176));
	gradient.setColorAt(1.00, QColor(255, 214, 230));

	auto pen = QPen(QBrush(gradient), width);
	pen.setCapStyle(Qt::RoundCap);
	p.setPen(pen);
	p.setBrush(Qt::NoBrush);
	p.drawEllipse(center, radius, radius);

	auto petal = QColor(255, 178, 204, 215);
	p.setBrush(petal);
	p.setPen(Qt::NoPen);
	for (auto i = 0; i != 8; ++i) {
		p.save();
		p.translate(center);
		p.rotate(i * 45. + (i % 2 ? 16. : 0.));
		p.drawEllipse(
			QPointF(0., -radius),
			width * 0.85,
			width * 1.45);
		p.restore();
	}
}

void PaintVoid(QPainter &p, const QRect &rect) {
	const auto size = rect.width();
	const auto center = Center(rect);
	const auto radius = size / 2. - std::max(1., size * 0.014);
	const auto width = std::max(1.6, size * 0.05);

	auto gradient = QConicalGradient(center, 210.);
	gradient.setColorAt(0.00, QColor(168, 120, 255));
	gradient.setColorAt(0.40, QColor(66, 32, 120));
	gradient.setColorAt(0.70, QColor(20, 12, 38));
	gradient.setColorAt(1.00, QColor(168, 120, 255));

	PaintGlow(p, center, radius, width, QColor(130, 82, 230), 0.30);

	auto pen = QPen(QBrush(gradient), width);
	pen.setCapStyle(Qt::FlatCap);
	auto dashes = QList<qreal>();
	dashes << 2.2 << 0.9 << 3.4 << 0.7;
	pen.setDashPattern(dashes);
	p.setPen(pen);
	p.setBrush(Qt::NoBrush);
	p.drawEllipse(center, radius, radius);

	p.setPen(Qt::NoPen);
	p.setBrush(QColor(190, 156, 255, 190));
	for (auto i = 0; i != 3; ++i) {
		p.save();
		p.translate(center);
		p.rotate(120. * i + 24.);
		auto shard = QPainterPath();
		shard.moveTo(0., -radius - width * 0.1);
		shard.lineTo(width * 0.55, -radius + width * 1.5);
		shard.lineTo(-width * 0.55, -radius + width * 1.5);
		shard.closeSubpath();
		p.drawPath(shard);
		p.restore();
	}
}

void PaintFrost(QPainter &p, const QRect &rect) {
	const auto size = rect.width();
	const auto center = Center(rect);
	const auto radius = size / 2. - std::max(1., size * 0.016);
	const auto width = std::max(1.6, size * 0.048);

	auto gradient = QLinearGradient(rect.topLeft(), rect.bottomRight());
	gradient.setColorAt(0.00, QColor(226, 250, 255));
	gradient.setColorAt(0.45, QColor(126, 206, 255));
	gradient.setColorAt(1.00, QColor(38, 106, 196));

	PaintGlow(p, center, radius, width, QColor(140, 220, 255), 0.26);

	auto pen = QPen(QBrush(gradient), width);
	pen.setCapStyle(Qt::RoundCap);
	p.setPen(pen);
	p.setBrush(Qt::NoBrush);
	p.drawEllipse(center, radius, radius);

	p.setPen(Qt::NoPen);
	p.setBrush(QColor(236, 252, 255, 210));
	for (auto i = 0; i != 6; ++i) {
		p.save();
		p.translate(center);
		p.rotate(i * 60. + 15.);
		auto spike = QPainterPath();
		spike.moveTo(0., -radius + width * 0.2);
		spike.lineTo(width * 0.45, -radius + width * 1.7);
		spike.lineTo(-width * 0.45, -radius + width * 1.7);
		spike.closeSubpath();
		p.drawPath(spike);
		p.restore();
	}
}

void PaintRainbow(QPainter &p, const QRect &rect) {
	const auto size = rect.width();
	const auto center = Center(rect);
	const auto radius = size / 2. - std::max(1., size * 0.014);
	const auto width = std::max(2., size * 0.065);

	auto gradient = QConicalGradient(center, 0.);
	gradient.setColorAt(0.00, QColor(255, 92, 92));
	gradient.setColorAt(0.16, QColor(255, 196, 84));
	gradient.setColorAt(0.33, QColor(120, 240, 140));
	gradient.setColorAt(0.50, QColor(84, 220, 255));
	gradient.setColorAt(0.66, QColor(110, 130, 255));
	gradient.setColorAt(0.83, QColor(214, 120, 255));
	gradient.setColorAt(1.00, QColor(255, 92, 92));

	PaintGlow(p, center, radius, width, QColor(255, 255, 255), 0.18);

	auto pen = QPen(QBrush(gradient), width);
	pen.setCapStyle(Qt::RoundCap);
	p.setPen(pen);
	p.setBrush(Qt::NoBrush);
	p.drawEllipse(center, radius, radius);

	auto inner = QPen(QColor(255, 255, 255, 160), std::max(0.8, width * 0.12));
	p.setPen(inner);
	p.drawEllipse(center, radius - width * 0.75, radius - width * 0.75);
}

} // namespace

Kind ForPeer(const PeerData *peer) {
	if (!peer || !peer->isUser() || peer->id.value == 0) {
		return Kind::None;
	}
	auto &registry = Registry::Instance();
	registry.EnsureStarted();
	return registry.For(peerToUser(peer->id).bare);
}

void Paint(QPainter &p, const QRect &rect, Kind kind) {
	if (kind == Kind::None || rect.width() < 12) {
		return;
	}
	p.save();
	p.setRenderHint(QPainter::Antialiasing, true);
	p.setBrush(Qt::NoBrush);

	switch (kind) {
	case Kind::Scratch: PaintScratch(p, rect); break;
	case Kind::Ember: PaintEmber(p, rect); break;
	case Kind::Neon: PaintNeon(p, rect); break;
	case Kind::Gold: PaintGold(p, rect); break;
	case Kind::Sakura: PaintSakura(p, rect); break;
	case Kind::Void: PaintVoid(p, rect); break;
	case Kind::Frost: PaintFrost(p, rect); break;
	case Kind::Rainbow: PaintRainbow(p, rect); break;
	case Kind::None: break;
	}

	p.restore();
}

} // namespace Deltagram::Frames
