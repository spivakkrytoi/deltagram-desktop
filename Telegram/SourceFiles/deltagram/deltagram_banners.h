/*
This file is part of DeltaGram Desktop,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "data/data_peer_id.h"

#include <QtGui/QImage>
#include <QtCore/QRect>

class PeerData;
class QPainter;

namespace Deltagram::Banners {

// Banner image for a peer, or a null image when the account has none.
// Starts the registry refresh lazily on the main thread.
[[nodiscard]] QImage ForPeer(const PeerData *peer);

// Draws a banner as a cover inside the given rectangle.
void Paint(QPainter &p, const QRect &rect, const QImage &image);

} // namespace Deltagram::Banners
