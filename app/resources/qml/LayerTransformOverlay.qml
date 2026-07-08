import QtQuick

// Layer Transform overlay.
//
// STAGE 1 (done): click-to-select. Clicking an overlay layer's bounding box
// on the canvas sets documentController.selectedLayerId; clicking empty
// space clears it. Selected/unselected layers get a thin border so their
// clickable regions are visible while the Transform tool is active.
//
// STAGE 2 (this file): drag-to-move.
//
// ── Root-cause fix for the "drag lags / resets every tick" bug ───────────
// The first version of this file put a MouseArea INSIDE each Repeater
// delegate (one per layer) and tracked the drag gesture (pressRootX,
// startPosX, dragging, etc.) as properties on THAT MouseArea. That looked
// reasonable but was wrong for a subtle reason:
//
//   Every setLayerTransform() call emits DocumentModel::changed(), which
//   (via DocumentController's forwarding lambda) emits layersChanged().
//   documentController.layerModel is a plain Q_PROPERTY(QVariantList), so
//   reading it again returns a BRAND NEW QVariantList/QVariantMap set --
//   not the same object, just similar data. This file's own
//   `overlayModel` then builds ANOTHER brand new JS array from that.
//
//   QML's Repeater, when bound to a plain JS array/var model (not a
//   ListModel or QAbstractItemModel), does not do identity-preserving
//   diffing across model reassignment -- it treats a new array object as
//   an entirely new model and destroys + recreates ALL of its delegate
//   Items. That includes whatever MouseArea lives inside each delegate,
//   along with all of ITS properties (pressRootX, startPosX, dragging...)
//   and its mouse grab.
//
//   So the previous design was destroying the very MouseArea that was in
//   the middle of tracking the drag, on every single mouse-move tick
//   (since every tick calls setLayerTransform(), which rebuilds the
//   model, which tears down the Repeater's delegates). The drag would
//   move a tiny amount, then silently lose its grab and reset -- exactly
//   the "one short swipe per click-and-drag" symptom.
//
// The fix: hoist the ENTIRE interactive gesture (hit-testing, press,
// drag-tracking) out of the Repeater into a single MouseArea covering the
// whole overlay (`interactionArea` below), which is NOT part of the
// Repeater and is therefore never destroyed by a model rebuild. Its
// pressX/startPosX/dragging state survives every tick of the drag, because
// the Item holding that state is stable for the whole gesture. The
// Repeater is now purely presentational -- it just draws each layer's box
// + selection border from the model, with no internal state of its own,
// so it's completely fine for it to be torn down and rebuilt every tick
// (it was never the thing tracking the drag).
//
// Because interactionArea itself never moves or gets recreated, mouse.x/
// mouse.y reported by IT are already a stable, absolute coordinate frame
// for the whole drag -- no mapToItem()/coordinate-remapping needed at all
// (the previous version's mapToItem() call was solving a real problem in
// principle, but on the wrong Item; it can't help if the MouseArea doing
// the measuring keeps getting destroyed and losing its own stored
// reference point).
//
// Hit-testing is now manual JS (hitTest() below) rather than relying on
// Qt Quick's native per-Item rotation-aware hit-testing (which stage 1
// used, back when each layer had its own MouseArea). It replicates the
// exact placement convention RenderPipeline::compositeOverlayLayers()
// uses (posX/posY in base-image pixels relative to canvas center; scaleX/
// scaleY relative to native pixel size; rotation in degrees clockwise),
// and un-rotates the click point around each box's center before the
// bounds check, so rotated layers (stage 4, not yet built) will still
// hit-test correctly once rotation exists.
Item {
    id: root
    property var docCtrl: null

    // Canvas-pixels-per-source-pixel -- this overlay is sized to match
    // imagePreview (sourceWidth/Height * zoom), so this recovers the same
    // factor RenderPipeline calls `previewScale`.
    readonly property real canvasScale: (docCtrl && docCtrl.sourceWidth > 0)
        ? width / docCtrl.sourceWidth : 1.0

    // documentController.layerModel is topmost-first; reverse + drop the
    // base layer so index 0 = lowest overlay order, last = topmost. Used
    // both for Repeater paint order and for hit-test priority below
    // (checked topmost-first).
    readonly property var overlayModel: {
        const list = docCtrl ? docCtrl.layerModel : [];
        const result = [];
        for (let i = list.length - 1; i >= 0; --i)
            if (!list[i].isBase) result.push(list[i]);
        return result;
    }

    // Single, STABLE MouseArea for the whole overlay -- see file header.
    // This is what makes the drag gesture survive model rebuilds: it is a
    // static child of `root`, never inside the Repeater, so it is never
    // destroyed mid-drag the way a Repeater-delegate's own MouseArea was.
    MouseArea {
        id: interactionArea
        anchors.fill: parent
        cursorShape: dragging ? Qt.SizeAllCursor : Qt.ArrowCursor

        property string dragLayerId: ""
        property real   pressX:     0
        property real   pressY:     0
        property real   startPosX:  0
        property real   startPosY:  0
        property bool   dragging:   false

        // Topmost-first hit test against each overlay layer's (possibly
        // rotated) bounding box, in this Item's own (== root's) local
        // coordinates.
        function hitTest(px, py) {
            const list = root.overlayModel;
            for (let i = list.length - 1; i >= 0; --i) {
                const l = list[i];
                const w  = Math.max(1, l.imgWidth  * l.scaleX * root.canvasScale);
                const h  = Math.max(1, l.imgHeight * l.scaleY * root.canvasScale);
                const cx = root.width  * 0.5 + l.posX * root.canvasScale;
                const cy = root.height * 0.5 + l.posY * root.canvasScale;
                const dx = px - cx, dy = py - cy;
                // Un-rotate the click point by -rotation around the box's
                // center, then test against the axis-aligned box.
                const rad  = -l.rotation * Math.PI / 180;
                const cosR = Math.cos(rad), sinR = Math.sin(rad);
                const localX = dx * cosR - dy * sinR;
                const localY = dx * sinR + dy * cosR;
                if (Math.abs(localX) <= w / 2 && Math.abs(localY) <= h / 2)
                    return l;
            }
            return null;
        }

        onPressed: (mouse) => {
            const hit = hitTest(mouse.x, mouse.y);
            if (root.docCtrl) root.docCtrl.selectedLayerId = hit ? hit.realId : "";
            dragLayerId = hit ? hit.realId : "";
            pressX = mouse.x; pressY = mouse.y;
            if (hit) { startPosX = hit.posX; startPosY = hit.posY; }
            dragging = false;
        }
        onPositionChanged: (mouse) => {
            if (!pressed || dragLayerId.length === 0) return;
            const dx = mouse.x - pressX;
            const dy = mouse.y - pressY;
            // Dead-zone: a plain click (no real movement) must not emit a
            // spurious near-zero setLayerTransform call.
            if (!dragging && Math.abs(dx) < 2 && Math.abs(dy) < 2) return;
            dragging = true;
            // Re-read current scale/rotation each tick rather than caching
            // a copy from press-time, in case they change mid-drag from
            // elsewhere (e.g. a resize handle in a later stage).
            let cur = null;
            for (const l of root.overlayModel) { if (l.realId === dragLayerId) { cur = l; break; } }
            if (!cur || !root.docCtrl) return;
            const newPosX = startPosX + dx / root.canvasScale;
            const newPosY = startPosY + dy / root.canvasScale;
            root.docCtrl.setLayerTransform(dragLayerId, newPosX, newPosY,
                                            cur.scaleX, cur.scaleY, cur.rotation);
        }
        onReleased: {
            dragLayerId = "";
            dragging = false;
        }
    }

    Repeater {
        model: root.overlayModel
        delegate: Item {
            id: handle
            readonly property bool isSelected: root.docCtrl && root.docCtrl.selectedLayerId === modelData.realId

            width:  Math.max(1, modelData.imgWidth  * modelData.scaleX * root.canvasScale)
            height: Math.max(1, modelData.imgHeight * modelData.scaleY * root.canvasScale)
            x: (root.width  * 0.5 + modelData.posX * root.canvasScale) - width  * 0.5
            y: (root.height * 0.5 + modelData.posY * root.canvasScale) - height * 0.5
            rotation: modelData.rotation
            transformOrigin: Item.Center

            // Purely presentational: no MouseArea, no state of its own. It
            // is fine for this Item to be destroyed and recreated on every
            // model rebuild (i.e. every drag tick) -- unlike the previous
            // design, nothing here needs to survive across ticks.
            Rectangle {
                anchors.fill: parent
                color: "transparent"
                border.width: handle.isSelected ? 2 : 1
                border.color: handle.isSelected ? "#6366f1" : "#ffffff33"
            }
        }
    }
}
