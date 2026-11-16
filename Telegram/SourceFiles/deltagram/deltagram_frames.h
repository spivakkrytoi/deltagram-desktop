/*
This file is part of DeltaGram Desktop,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QRect>
#include <QtGui/QPainter>

class PeerData;

namespace Deltagram::Frames {

// Cosmetic avatar frames granted by operators through the DeltaGram admin
// panel. The ids must stay in sync with internal/domain/avatar_frame.go on the
// server side.
enum class Kind {
	None,
	Scratch,
	Ember,
	Neon,
	Gold,
	Sakura,
	Void,
	Frost,
	Rainbow,
};

// Returns the frame granted to a peer, or Kind::None. Only ordinary users can
// carry a frame; channels, groups and bots always return Kind::None.
[[nodiscard]] Kind ForPeer(const PeerData *peer);

// Paints the frame ring inside `rect` (a square avatar bounds). The frame is
// drawn on top of the already painted avatar so it can overlap the edge
// slightly without growing the widget.
void Paint(QPainter &p, const QRect &rect, Kind kind);

} // namespace Deltagram::Frames
