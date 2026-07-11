import QtQuick

// Layer Transform overlay.
//
// STAGE 1 (done): click-to-select. Clicking an overlay layer's bounding box
// on the canvas sets documentController.selectedLayerId; clicking empty
// space clears it. Selected/unselected layers get a thin border so their
// clickable regions are visible while the Transform tool is active.
//
// STAGE 2 (done): drag-to-move.
//
// ── Root-cause fix for the "drag lags / resets every tick" bug ───────────
// The first version of this file put a MouseArea INSIDE each Repeater
// delegate (one per layer) and tracked the drag gesture as properties on
// THAT MouseArea. That was wrong: every setLayerTransform() call emits
// changed() -> layersChanged() -> documentController.layerModel is re-read
// -> a BRAND NEW QVariantList comes back every time -> this file's own
// overlayModel builds another brand-new JS array -> QML's Repeater (bound
// to a plain JS array, no identity-preserving diff) destroys and recreates
// ALL delegates on every tick, including whatever MouseArea/state lived
// inside them. The drag would move a tiny amount, then silently lose its
// grab and reset.
//
// The fix, still in effect for stage 3: the ENTIRE interactive gesture
// (hit-testing, press, drag-tracking, resize-tracking) lives in a single
// MouseArea covering the whole overlay (`interactionArea` below), which is
// NOT part of any Repeater and is therefore never destroyed by a model
// rebuild. Everything else (box borders, resize-handle squares) is purely
// presentational, safe to rebuild every tick, since none of it holds state
// that needs to survive across ticks.
//
// STAGE 3 (this file): resize handles for the selected layer.
// 8 handles (4 corners + 4 edges), matching CropOverlay.qml's handle
// layout. Corner handles resize both axes; aspect ratio is LOCKED by
// default (uniform scale, derived by projecting the mouse onto the box's
// diagonal from the fixed/opposite corner -- the standard technique for
// "closest uniform scale to an arbitrary drag direction"), with Shift held
// as the free-transform escape hatch (independent X/Y resize). Edge
// handles always resize a single axis only, same as CropOverlay.
//
// All resize math is done in the box's own LOCAL (unrotated) coordinate
// frame -- the mouse position is un-rotated into that frame first (same
// inverse-rotation used by hitTest()), the new box bounds are computed
// there, and only the resulting CENTER OFFSET is rotated back out to
// world/canvas space at the end. Rotation is always 0 today (stage 4, not
// yet built, is the only way to set it), so in practice every cosR/sinR
// below is currently 1/0 and this reduces to plain axis-aligned math --
// but it means stage 4 does not need to come back and redo this file's
// resize math once rotation exists.
//
// Resize (like move) is bracketed with begin/commitLayerTransformEdit(),
// so a whole resize drag is one undo step, reusing the exact same
// transaction machinery as move -- no separate mechanism.
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

    // Full current data for whichever layer is selected, or null. Re-reads
    // overlayModel fresh each time (including every tick during a drag or
    // resize), so both the visual handles/box and any in-progress gesture
    // math always see the layer's latest posX/posY/scaleX/scaleY.
    readonly property var selectedLayerData: {
        if (!docCtrl) return null;
        const id = docCtrl.selectedLayerId;
        if (!id) return null;
        for (const l of overlayModel) if (l.realId === id) return l;
        return null;
    }

    // Single, STABLE MouseArea for the whole overlay -- see file header.
    // Holds ALL interaction state (move drag AND resize drag) so nothing
    // is ever lost to a mid-gesture model rebuild.
    MouseArea {
        id: interactionArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: {
            if (mode === "resize") return cursorForHandle(activeHandle);
            if (mode === "move")   return Qt.SizeAllCursor;
            // Not currently dragging: preview a resize cursor on hover over
            // a handle, otherwise default arrow.
            const hovered = root.selectedLayerData ? hitTestHandle(mouseX, mouseY) : "";
            return hovered.length > 0 ? cursorForHandle(hovered) : Qt.ArrowCursor;
        }

        property string dragLayerId:  ""
        property string mode:         ""      // "" | "move" | "resize"
        property string activeHandle: ""       // "" | nw/n/ne/e/se/s/sw/w
        property real   pressX:       0
        property real   pressY:       0
        property real   startPosX:    0
        property real   startPosY:    0
        property real   startScaleX:  1
        property real   startScaleY:  1
        property real   startRotation:0
        property real   startImgW:    0
        property real   startImgH:    0
        property bool   freeTransform:false    // Shift held at press time (corner handles only)
        property bool   dragging:     false

        function cursorForHandle(h) {
            switch (h) {
            case "nw": case "se": return Qt.SizeFDiagCursor;
            case "ne": case "sw": return Qt.SizeBDiagCursor;
            case "n":  case "s":  return Qt.SizeVerCursor;
            case "e":  case "w":  return Qt.SizeHorCursor;
            default:               return Qt.ArrowCursor;
            }
        }

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

        // Returns which of the 8 handles (if any) of the CURRENTLY
        // SELECTED layer is under (px, py), using a small circular hit
        // radius around each handle's world position (rotation-aware: the
        // handle's local offset is rotated by the layer's own rotation
        // before comparing against the click point).
        function hitTestHandle(px, py) {
            const l = root.selectedLayerData;
            if (!l) return "";
            const hw = Math.max(1, l.imgWidth  * l.scaleX * root.canvasScale) / 2;
            const hh = Math.max(1, l.imgHeight * l.scaleY * root.canvasScale) / 2;
            const cx = root.width  * 0.5 + l.posX * root.canvasScale;
            const cy = root.height * 0.5 + l.posY * root.canvasScale;
            const rad = l.rotation * Math.PI / 180;
            const cosR = Math.cos(rad), sinR = Math.sin(rad);
            const handles = [
                ["nw", -hw, -hh], ["n", 0, -hh], ["ne", hw, -hh], ["e", hw, 0],
                ["se", hw, hh],   ["s", 0, hh],   ["sw", -hw, hh], ["w", -hw, 0]
            ];
            const HIT_R = 9;
            for (const [name, lx, ly] of handles) {
                const wx = cx + (lx * cosR - ly * sinR);
                const wy = cy + (lx * sinR + ly * cosR);
                if (Math.hypot(px - wx, py - wy) <= HIT_R) return name;
            }
            return "";
        }

        onPressed: (mouse) => {
            const handle = root.selectedLayerData ? hitTestHandle(mouse.x, mouse.y) : "";
            if (handle.length > 0) {
                const l = root.selectedLayerData;
                mode          = "resize";
                activeHandle  = handle;
                dragLayerId   = l.realId;
                startPosX     = l.posX;    startPosY     = l.posY;
                startScaleX   = l.scaleX;  startScaleY   = l.scaleY;
                startRotation = l.rotation;
                startImgW     = l.imgWidth; startImgH    = l.imgHeight;
                freeTransform = (mouse.modifiers & Qt.ShiftModifier) !== 0;
                pressX = mouse.x; pressY = mouse.y;
                dragging = false;
                // Brackets the whole resize gesture into one undo step --
                // same mechanism as move, see onReleased.
                if (root.docCtrl) root.docCtrl.beginLayerTransformEdit();
                return;
            }

            const hit = hitTest(mouse.x, mouse.y);
            mode = hit ? "move" : "";
            if (root.docCtrl) root.docCtrl.selectedLayerId = hit ? hit.realId : "";
            dragLayerId = hit ? hit.realId : "";
            pressX = mouse.x; pressY = mouse.y;
            if (hit) {
                startPosX = hit.posX; startPosY = hit.posY;
                // Brackets the whole gesture (however many setLayerTransform()
                // ticks it produces) into a single undo step, mirroring how
                // Main.qml's AdjustmentSlider calls beginAdjustmentEdit()/
                // commitAdjustmentEdit() on press/release. Safe even if the
                // press turns out to be a plain click with no drag --
                // DocumentModel's transactionChangedAnything() (now
                // layer-aware) detects nothing changed and skips the undo
                // step, same as it already does for a no-op slider press.
                if (root.docCtrl) root.docCtrl.beginLayerTransformEdit();
            }
            dragging = false;
        }

        onPositionChanged: (mouse) => {
            if (!pressed || dragLayerId.length === 0) return;
            if (mode === "resize") { doResize(mouse.x, mouse.y); return; }

            const dx = mouse.x - pressX;
            const dy = mouse.y - pressY;
            // Dead-zone: a plain click (no real movement) must not emit a
            // spurious near-zero setLayerTransform call.
            if (!dragging && Math.abs(dx) < 2 && Math.abs(dy) < 2) return;
            dragging = true;
            // Re-read current scale/rotation each tick rather than caching
            // a copy from press-time, in case they change mid-drag from
            // elsewhere.
            let cur = null;
            for (const l of root.overlayModel) { if (l.realId === dragLayerId) { cur = l; break; } }
            if (!cur || !root.docCtrl) return;
            const newPosX = startPosX + dx / root.canvasScale;
            const newPosY = startPosY + dy / root.canvasScale;
            root.docCtrl.setLayerTransform(dragLayerId, newPosX, newPosY,
                                            cur.scaleX, cur.scaleY, cur.rotation);
        }

        // Minimum box size, in canvas pixels, that a resize is allowed to
        // shrink to -- prevents a drag past the opposite corner/edge from
        // producing a degenerate zero/negative/inverted layer.
        readonly property real minSizePx: 12

        function doResize(mouseX, mouseY) {
            if (!root.docCtrl) return;
            dragging = true;

            const startW = Math.max(1, startImgW * startScaleX * root.canvasScale);
            const startH = Math.max(1, startImgH * startScaleY * root.canvasScale);
            const startHw = startW / 2, startHh = startH / 2;
            const startCx = root.width  * 0.5 + startPosX * root.canvasScale;
            const startCy = root.height * 0.5 + startPosY * root.canvasScale;

            // Un-rotate the current mouse position into the box's own
            // local (unrotated) frame, centered on the box's PRESS-TIME
            // center -- same technique as hitTest()/hitTestHandle().
            const rad  = -startRotation * Math.PI / 180;
            const cosR = Math.cos(rad), sinR = Math.sin(rad);
            const ddx = mouseX - startCx, ddy = mouseY - startCy;
            const localMouseX = ddx * cosR - ddy * sinR;
            const localMouseY = ddx * sinR + ddy * cosR;

            // Local-frame box bounds, relative to the PRESS-TIME center.
            // Start from the unchanged box, then let the active handle
            // override whichever edges it controls.
            let left = -startHw, right = startHw, top = -startHh, bottom = startHh;

            const isCorner = (activeHandle === "nw" || activeHandle === "ne" ||
                               activeHandle === "se" || activeHandle === "sw");

            if (isCorner && !freeTransform) {
                // Aspect-locked corner resize: project the mouse (in local
                // frame) onto the diagonal from the FIXED (opposite)
                // corner to the corner being dragged, and use that
                // projection as a single uniform scale factor `t`. This is
                // the standard "closest uniform scale to an arbitrary drag
                // direction" technique -- dragging exactly along the
                // diagonal gives an exact scale; dragging off-diagonal
                // still gives a sensible, stable uniform scale rather than
                // fighting between two different per-axis factors.
                const fixedX = (activeHandle === "ne" || activeHandle === "se") ? -startHw : startHw;
                const fixedY = (activeHandle === "se" || activeHandle === "sw") ? -startHh : startHh;
                const draggedX = -fixedX, draggedY = -fixedY;
                const diagX = draggedX - fixedX, diagY = draggedY - fixedY;
                const vX = localMouseX - fixedX, vY = localMouseY - fixedY;
                const diagLenSq = diagX * diagX + diagY * diagY;
                let t = diagLenSq > 0 ? (vX * diagX + vY * diagY) / diagLenSq : 1.0;
                t = Math.max(0.05, t); // never let the uniform scale collapse to ~0 or invert
                const newDraggedX = fixedX + t * diagX;
                const newDraggedY = fixedY + t * diagY;
                left   = Math.min(fixedX, newDraggedX); right  = Math.max(fixedX, newDraggedX);
                top    = Math.min(fixedY, newDraggedY); bottom = Math.max(fixedY, newDraggedY);
            } else {
                // Free (non-uniform) resize: each edge the active handle
                // touches moves independently to the mouse; edges it
                // doesn't touch stay at their press-time position.
                if (activeHandle.indexOf("w") !== -1) left   = localMouseX;
                if (activeHandle.indexOf("e") !== -1) right  = localMouseX;
                if (activeHandle.indexOf("n") !== -1) top    = localMouseY;
                if (activeHandle.indexOf("s") !== -1) bottom = localMouseY;
            }

            let newW = right - left, newH = bottom - top;
            // Clamp to the minimum size, anchored at whichever side is
            // NOT being dragged (so the fixed edge/corner truly stays put
            // even once clamped).
            if (newW < minSizePx) {
                if (activeHandle.indexOf("w") !== -1) left  = right - minSizePx;
                else                                   right = left + minSizePx;
                newW = minSizePx;
            }
            if (newH < minSizePx) {
                if (activeHandle.indexOf("n") !== -1) top    = bottom - minSizePx;
                else                                   bottom = top + minSizePx;
                newH = minSizePx;
            }

            // New local-frame center offset from the press-time center,
            // rotated back out to world/canvas space.
            const localOffsetX = (left + right) / 2;
            const localOffsetY = (top + bottom) / 2;
            const fRad = startRotation * Math.PI / 180;
            const fCos = Math.cos(fRad), fSin = Math.sin(fRad);
            const worldOffsetX = localOffsetX * fCos - localOffsetY * fSin;
            const worldOffsetY = localOffsetX * fSin + localOffsetY * fCos;

            const newCx = startCx + worldOffsetX;
            const newCy = startCy + worldOffsetY;
            const newPosX = (newCx - root.width  * 0.5) / root.canvasScale;
            const newPosY = (newCy - root.height * 0.5) / root.canvasScale;
            const newScaleX = newW / (startImgW * root.canvasScale);
            const newScaleY = newH / (startImgH * root.canvasScale);

            root.docCtrl.setLayerTransform(dragLayerId, newPosX, newPosY,
                                            newScaleX, newScaleY, startRotation);
        }

        onReleased: {
            // Safe to call unconditionally even if beginLayerTransformEdit()
            // was never called this press (e.g. clicked empty space) --
            // DocumentController guards on its own m_layerTransformEditOpen
            // flag and no-ops.
            if (root.docCtrl) root.docCtrl.commitLayerTransformEdit();
            dragLayerId  = "";
            mode         = "";
            activeHandle = "";
            dragging     = false;
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

    // Resize-handle squares for the SELECTED layer only. Purely
    // presentational (no MouseArea/state of their own -- all interaction
    // lives in interactionArea above), so it's fine for these to rebuild
    // every tick. The inner Repeater's model is a fixed 8-element JS array
    // literal (not derived from docCtrl.layerModel), so it never changes
    // identity and Qt Quick never destroys/recreates these delegates on
    // its own account either way -- doubly safe.
    Item {
        id: handleAnchor
        visible: root.selectedLayerData !== null
        width: 0
        height: 0
        x: root.selectedLayerData ? (root.width  * 0.5 + root.selectedLayerData.posX * root.canvasScale) : 0
        y: root.selectedLayerData ? (root.height * 0.5 + root.selectedLayerData.posY * root.canvasScale) : 0
        rotation: root.selectedLayerData ? root.selectedLayerData.rotation : 0

        readonly property real hw: root.selectedLayerData
            ? Math.max(1, root.selectedLayerData.imgWidth  * root.selectedLayerData.scaleX * root.canvasScale) / 2 : 0
        readonly property real hh: root.selectedLayerData
            ? Math.max(1, root.selectedLayerData.imgHeight * root.selectedLayerData.scaleY * root.canvasScale) / 2 : 0

        Repeater {
            model: [
                { lx: -1, ly: -1 }, { lx: 0, ly: -1 }, { lx: 1, ly: -1 },
                { lx: 1,  ly: 0  },
                { lx: 1,  ly: 1  }, { lx: 0, ly: 1 }, { lx: -1, ly: 1 },
                { lx: -1, ly: 0  }
            ]
            delegate: Rectangle {
                width: 10; height: 10; radius: 2
                color: "#ffffff"
                border.color: "#6366f1"
                border.width: 1.5
                x: handleAnchor.hw * modelData.lx - width  / 2
                y: handleAnchor.hh * modelData.ly - height / 2
            }
        }
    }
}
